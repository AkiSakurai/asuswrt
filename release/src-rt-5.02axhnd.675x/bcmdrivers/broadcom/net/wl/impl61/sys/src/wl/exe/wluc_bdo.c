/*
 * wl bdo command module - Bonjour Dongle Offload
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
 * $Id: wluc_bdo.c 774769 2019-05-07 08:46:22Z $
 */

#include <wlioctl.h>

#if defined(DONGLEBUILD)
#include <typedefs.h>
#include <osl.h>
#endif // endif

#include <sys/stat.h>
#include <errno.h>

#include <bcmutils.h>
#include <bcmendian.h>
#include "wlu_common.h"
#include "wlu.h"

static cmd_func_t wl_bdo;

static cmd_t wl_bdo_cmds[] = {
	{ "bdo", wl_bdo, WLC_GET_VAR, WLC_SET_VAR,
	"Bonjour dongle offload subcommands:\n"
	"\tbdo download <dbase_filename>\n"
	"\tbdo enable <0|1>\n"
	"\tbdo max_download"},
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_bdo_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register bdo commands */
	wl_module_cmds_register(wl_bdo_cmds);
}

/* download database from buffer */
static int
wl_bdo_download_database(void *wl, uint8 *database, uint16 len)
{
	int err = BCME_OK;
	uint16 total_size = len;
	uint16 current_size = 0;
	uint16 frag_num = 0;

	while (current_size < total_size) {
		uint8 buffer[OFFSETOF(wl_bdo_t, data) + sizeof(wl_bdo_download_t)];
		wl_bdo_t *bdo = (wl_bdo_t *)buffer;
		int length;
		wl_bdo_download_t *bdo_download = (wl_bdo_download_t *)bdo->data;

		/* update size and fragment */
		bdo_download->total_size = total_size;
		bdo_download->frag_num = frag_num;
		bdo_download->frag_size = MIN(total_size - current_size, BDO_MAX_FRAGMENT_SIZE);
		memcpy(bdo_download->fragment, &database[current_size], bdo_download->frag_size);

		bdo->subcmd_id = WL_BDO_SUBCMD_DOWNLOAD;
		bdo->len = OFFSETOF(wl_bdo_download_t, fragment) + bdo_download->frag_size;

		/* update before dongle byte order */
		current_size += bdo_download->frag_size;
		frag_num++;

		/* dongle byte order */
		bdo_download->total_size = htod16(bdo_download->total_size);
		bdo_download->frag_num = htod16(bdo_download->frag_num);
		bdo_download->frag_size = htod16(bdo_download->frag_size);

		/* invoke iovar */
		length = OFFSETOF(wl_bdo_t, data) + bdo->len;
		bdo->subcmd_id = htod16(bdo->subcmd_id);
		bdo->len = htod16(bdo->len);
		if (wlu_iovar_setbuf(wl, "bdo", bdo, length, buf, WLC_IOCTL_MAXLEN) != 0) {
			err = BCME_ERROR;
			break;
		}
	}
	return err;
}

/* reads file into buffer and returns bytes read and malloc'ed buffer with file contents */
static int
read_file(char *filename, unsigned char **buffer)
{
	FILE *fp = NULL;
	int ifd;
	struct stat filest;
	unsigned int filesize = 0;
	unsigned long status = 0;
	unsigned char *buf = NULL;

	/* open the file */
	if (!(fp = fopen(filename, "rb"))) {
		fprintf(stderr, "unable to open input file %s\n", filename);
		goto error;
	}

	/* get fstat */
	ifd = fileno(fp);
	if (fstat(ifd, &filest)) {
		fprintf(stderr, "fstat on input file %s return error %s\n",
			filename, strerror(errno));
		goto error;
	}

	/* get filesize */
	filesize = filest.st_size;
	if (filesize == 0) {
		fprintf(stderr, "input file %s is empty (i.e. zero length)\n", filename);
		goto error;
	}

	/* get buffer */
	if ((buf = malloc(filesize)) == NULL) {
		fprintf(stderr, "unable to allocate %u bytes based on input file size!\n",
			filesize);
		goto error;
	}

	/* read from file to buffer and check length */
	status = fread(buf, 1, filesize, fp);
	if (status != filesize) {
		fprintf(stderr, "read of input file %s wasn't good based on fstat size %u\n",
			filename, filesize);
		goto error;
	}

	/* return buffer and filesize */
	*buffer = buf;
	fclose(fp);
	return filesize;

error:
	if (buf) {
		free(buf);
	}
	if (fp) {
		fclose(fp);
	}
	return 0;
}

/* converts input hex buffer to binary and returns bytes converted and malloc'ed buffer */
static int
hex2bin(char *input, int len, unsigned char **buffer)
{
	unsigned char *buf = NULL;
	int num_converted = 0;
	char *p;

	/* get buffer */
	if ((buf = malloc(len)) == NULL) {
		fprintf(stderr, "unable to allocate %u bytes\n", len);
		return 0;
	}

	p = input;
	while (p < input + len) {
		char hex[3];

		/* ignore unknown leading/trailing characters */
		if (!isprint(*p)) {
			p++;
			continue;
		}

		hex[0] = *p++;
		hex[1] = *p++;
		hex[2] = 0;
		/* convert hex to binary */
		buf[num_converted++] = strtoul(hex, NULL, 16);
	}

	/* return buffer and number converted */
	*buffer = buf;
	return num_converted;
}

/* read database from file and download buffer */
static int
wl_bdo_download_file(void *wl, char *filename)
{
	int err;
	int file_len;
	unsigned char *file_buf;
	unsigned char *conv_buf = NULL;
	int dload_len;
	unsigned char *dload_buf;

	if ((file_len = read_file(filename, &file_buf)) == 0) {
		return BCME_BADARG;
	}

	/* conversion to binary is needed if first char is printable */
	if (isprint(file_buf[0])) {
		dload_len = hex2bin((char *)file_buf, file_len, &conv_buf);
		dload_buf = conv_buf;
	} else {
		dload_len = file_len;
		dload_buf = file_buf;
	}

	err = wl_bdo_download_database(wl, dload_buf, dload_len);
	if (err) {
		printf("Failed to download. Error code: %d\n", err);
	}
	else {
		printf("%s: %d bytes\n", filename, dload_len);
	}

	free(file_buf);
	if (conv_buf) {
		free(conv_buf);
	}

	return err;
}

static int
wl_bdo(void *wl, cmd_t *cmd, char **argv)
{
	int err = -1;
	char *subcmd;
	int subcmd_len;

	/* skip iovar */
	argv++;

	/* must have subcommand */
	subcmd = *argv++;
	if (!subcmd) {
		return BCME_USAGE_ERROR;
	}
	subcmd_len = strlen(subcmd);

	if (!*argv) {
		/* get */
		uint8 buffer[OFFSETOF(wl_bdo_t, data)];
		wl_bdo_t *bdo = (wl_bdo_t *)buffer;
		int len = OFFSETOF(wl_bdo_t, data);

		memset(bdo, 0, len);
		if (!strncmp(subcmd, "enable", subcmd_len)) {
			bdo->subcmd_id = WL_BDO_SUBCMD_ENABLE;
		} else if (!strncmp(subcmd, "max_download", subcmd_len)) {
			bdo->subcmd_id = WL_BDO_SUBCMD_MAX_DOWNLOAD;
		} else {
			return BCME_USAGE_ERROR;
		}

		/* invoke GET iovar */
		bdo->subcmd_id = htod16(bdo->subcmd_id);
		bdo->len = htod16(bdo->len);
		if ((err = wlu_iovar_getbuf(wl, cmd->name, bdo, len, buf, WLC_IOCTL_SMLEN)) < 0) {
			return err;
		}

		/* process and print GET results */
		bdo = (wl_bdo_t *)buf;
		bdo->subcmd_id = dtoh16(bdo->subcmd_id);
		bdo->len = dtoh16(bdo->len);

		switch (bdo->subcmd_id) {
		case WL_BDO_SUBCMD_ENABLE:
		{
			wl_bdo_enable_t *bdo_enable = (wl_bdo_enable_t *)bdo->data;
			if (bdo->len >= sizeof(*bdo_enable)) {
				printf("%d\n", bdo_enable->enable);
			} else {
				err = BCME_BADLEN;
			}
			break;
		}
		case WL_BDO_SUBCMD_MAX_DOWNLOAD:
		{
			wl_bdo_max_download_t *max_download = (wl_bdo_max_download_t *)bdo->data;
			if (bdo->len >= sizeof(*max_download)) {
				printf("%d\n", dtoh16(max_download->size));
			} else {
				err = BCME_BADLEN;
			}
			break;
		}
		default:
			break;
		}
	} else if (!strncmp(subcmd, "download", subcmd_len) && argv[0]) {
		/* download */
		err = wl_bdo_download_file(wl, argv[0]);
	}
	else {
		/* set */
		uint8 buffer[OFFSETOF(wl_bdo_t, data) +	sizeof(wl_bdo_enable_t)];
		wl_bdo_t *bdo = (wl_bdo_t *)buffer;
		int len;

		if (!strncmp(subcmd, "enable", subcmd_len) &&
			(!strcmp(argv[0], "0") || !strcmp(argv[0], "1"))) {
			wl_bdo_enable_t *bdo_enable = (wl_bdo_enable_t *)bdo->data;
			bdo->subcmd_id = WL_BDO_SUBCMD_ENABLE;
			bdo->len = sizeof(*bdo_enable);
			bdo_enable->enable = atoi(argv[0]);
		} else {
			return BCME_USAGE_ERROR;
		}

		/* invoke SET iovar */
		len = OFFSETOF(wl_bdo_t, data) + bdo->len;
		bdo->subcmd_id = htod16(bdo->subcmd_id);
		bdo->len = htod16(bdo->len);
		err = wlu_iovar_set(wl, cmd->name, bdo, len);
	}

	return err;
}
