/*
 * Common code for wl command-line swiss-army-knife utility
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
 * $Id: wluc_otp.c 774769 2019-05-07 08:46:22Z $
 */

#include <wlioctl.h>
#include <wlioctl_utils.h>

#if defined(DONGLEBUILD)
#include <typedefs.h>
#include <osl.h>
#endif // endif

#include <typedefs.h>
#include <epivers.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmwifi_channels.h>
#include <bcmsrom_fmt.h>
#include <bcmsrom_tbl.h>
#include "wlu_common.h"
#include "wlu.h"
#include <bcmcdc.h>
#if defined(linux)
#ifndef TARGETENV_android
#include <unistd.h>
#endif // endif
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <time.h>
#include <sched.h>
#define TIME_STR_SZ 100 /*  string buffer size for timestamp formatting */
#endif /* linux */
#if defined(WLBSSLOAD_REPORT) && defined(linux)
#include <sys/time.h>
#endif   /* defined(WLBSSLOAD_REPORT) && defined(linux) */

#ifdef LINUX
#include <inttypes.h>
#endif // endif
#include <miniopt.h>
#include <errno.h>

#if defined SERDOWNLOAD || defined CLMDOWNLOAD
#include <sys/stat.h>
#include <trxhdr.h>
#ifdef SERDOWNLOAD
#include <usbrdl.h>
#endif // endif
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#endif /* SERDOWNLOAD || defined CLMDOWNLOAD */

#include "wluc_otp.h"
#include <bcmnvram.h>

/* For backwards compatibility, the absense of the define 'NO_FILESYSTEM_SUPPORT'
* implies that a filesystem is supported.
*/
#if !defined(BWL_NO_FILESYSTEM_SUPPORT)
#define BWL_FILESYSTEM_SUPPORT
#endif // endif

static cmd_func_t wlu_cisdump;
cmd_func_t wl_otpw, wl_otpraw;
cmd_func_t wlu_ciswrite, wlu_cisupdate;
static cmd_func_t wlu_otpcrc;
cmd_func_t wlu_otpcrcconfig;
static cmd_func_t wl_otpdump_iter;
static cmd_func_t wl_var_setintandprintstr;
static cmd_func_t wl_otpecc_rows;
static cmd_func_t wl_otpecc_rowslock;
static cmd_func_t wl_otpecc_rowsdump;

typedef struct otp_raw {
	uint32 bit_offset;
	uint32 num_bits;
	uint32 crc;	/* Written in multiple of 32bits and bit should be 32bit alligned. */
} otp_raw_t;

#define OTPCRC_MFG_REGION_SIZE		64
#define OTPCRC_BITS_IN_BYTE		8
#define OTPCRC_CRC_WRITE_SIZE		(4 * OTPCRC_BITS_IN_BYTE)
#define OTPCRC_ALIGN_32_BIT_MASK	(~0x1F)
#define OTPCRC_ALIGN_4_BIT_MASK		(~0x03)
#define OTPCRC_CRC_VER_MASK		0xC000	/* bit 15:14 of CRC field */
#define OTPCRC_CRC_VER_SHIFT		14
#define OTPCRC_NUM_CRC_MASK		0x3800	/* bit 13:11 of CRC field */
#define OTPCRC_NUM_CRC_SHIFT		11
#define OTPCRC_END_ADDR_MASK		0x07FF	/* bit 10:0 of CRC field */
#define OTPCRC_END_ADDR_SHIFT		0
#define OTPCRC_OTPW_BUF_SIZE		1150
#define OTPCRC_REGION_SIZE_MAX		28
#define OTPCRC_VER_MAX			3
#define OTPCRC_REGION_SIZE_MULT		4

/*******************************************************************************
 * crc8
 *
 * Computes a crc8 over the input data using the polynomial:
 *
 *       x^8 + x^7 +x^6 + x^4 + x^2 + 1
 *
 * ****************************************************************************
 */

static const uint8 otpcrc8_table[256] = {
	0x00, 0xD5, 0x7F, 0xAA, 0xFE, 0x2B, 0x81, 0x54,
	0x29, 0xFC, 0x56, 0x83, 0xD7, 0x02, 0xA8, 0x7D,
	0x52, 0x87, 0x2D, 0xF8, 0xAC, 0x79, 0xD3, 0x06,
	0x7B, 0xAE, 0x04, 0xD1, 0x85, 0x50, 0xFA, 0x2F,
	0xA4, 0x71, 0xDB, 0x0E, 0x5A, 0x8F, 0x25, 0xF0,
	0x8D, 0x58, 0xF2, 0x27, 0x73, 0xA6, 0x0C, 0xD9,
	0xF6, 0x23, 0x89, 0x5C, 0x08, 0xDD, 0x77, 0xA2,
	0xDF, 0x0A, 0xA0, 0x75, 0x21, 0xF4, 0x5E, 0x8B,
	0x9D, 0x48, 0xE2, 0x37, 0x63, 0xB6, 0x1C, 0xC9,
	0xB4, 0x61, 0xCB, 0x1E, 0x4A, 0x9F, 0x35, 0xE0,
	0xCF, 0x1A, 0xB0, 0x65, 0x31, 0xE4, 0x4E, 0x9B,
	0xE6, 0x33, 0x99, 0x4C, 0x18, 0xCD, 0x67, 0xB2,
	0x39, 0xEC, 0x46, 0x93, 0xC7, 0x12, 0xB8, 0x6D,
	0x10, 0xC5, 0x6F, 0xBA, 0xEE, 0x3B, 0x91, 0x44,
	0x6B, 0xBE, 0x14, 0xC1, 0x95, 0x40, 0xEA, 0x3F,
	0x42, 0x97, 0x3D, 0xE8, 0xBC, 0x69, 0xC3, 0x16,
	0xEF, 0x3A, 0x90, 0x45, 0x11, 0xC4, 0x6E, 0xBB,
	0xC6, 0x13, 0xB9, 0x6C, 0x38, 0xED, 0x47, 0x92,
	0xBD, 0x68, 0xC2, 0x17, 0x43, 0x96, 0x3C, 0xE9,
	0x94, 0x41, 0xEB, 0x3E, 0x6A, 0xBF, 0x15, 0xC0,
	0x4B, 0x9E, 0x34, 0xE1, 0xB5, 0x60, 0xCA, 0x1F,
	0x62, 0xB7, 0x1D, 0xC8, 0x9C, 0x49, 0xE3, 0x36,
	0x19, 0xCC, 0x66, 0xB3, 0xE7, 0x32, 0x98, 0x4D,
	0x30, 0xE5, 0x4F, 0x9A, 0xCE, 0x1B, 0xB1, 0x64,
	0x72, 0xA7, 0x0D, 0xD8, 0x8C, 0x59, 0xF3, 0x26,
	0x5B, 0x8E, 0x24, 0xF1, 0xA5, 0x70, 0xDA, 0x0F,
	0x20, 0xF5, 0x5F, 0x8A, 0xDE, 0x0B, 0xA1, 0x74,
	0x09, 0xDC, 0x76, 0xA3, 0xF7, 0x22, 0x88, 0x5D,
	0xD6, 0x03, 0xA9, 0x7C, 0x28, 0xFD, 0x57, 0x82,
	0xFF, 0x2A, 0x80, 0x55, 0x01, 0xD4, 0x7E, 0xAB,
	0x84, 0x51, 0xFB, 0x2E, 0x7A, 0xAF, 0x05, 0xD0,
	0xAD, 0x78, 0xD2, 0x07, 0x53, 0x86, 0x2C, 0xF9,
};

uint8
otp_crc8(
	uint8 *pdata,	/* pointer to array of data to process */
	uint  nbytes,	/* number of input data bytes to process */
	uint8 crc	/* either CRC8_INIT_VALUE or previous return value */
)
{
	while (nbytes-- > 0)
		crc = otpcrc8_table[(crc ^ *pdata++) & 0xff];

	return crc;
}

#define OTPCRC_VALIDATE_INTEGRITY \
	if ((ret = wlu_get_crc_config(wl, &otpcrc_config)) == BCME_VALID_CRCCONFIG) { \
		integrity_chk = 1; \
		if ((ret = wlu_otpcrc_validate_otp(wl, &tmp_buf, &otpcrc_config, \
				&otpsize)) != BCME_OK) { \
			return ret; \
		} \
	} else { \
		if (ret == BCME_NO_CRCCONFIG) { \
			integrity_chk = 0; \
			ret = BCME_OK; \
		} else {\
			/* Configuration is programmmed but not valid. */ \
			return BCME_ERROR; \
		} \
	} \

/* Check if space is available for new CRC */
#define OTPCRC_CHECK_CRC_MEM_AVAILABILITY \
	if (integrity_chk == 1) { \
		if (wlu_is_crc_mem_avail(tmp_buf, &otpcrc_config) == FALSE) { \
			fprintf(stderr, "No space for new CRC\n"); \
			ret = BCME_ERROR; \
			goto done; \
		} \
	} \

#define OTPCRC_READ_AND_UPDATE_CRC \
	if (integrity_chk) { \
		memset(tmp_buf, 0, otpsize); \
		/* Read full OTP to calculate new CRC */ \
		if (wlu_read_otp_data(wl, tmp_buf, 0) != BCME_OK) { \
			fprintf(stderr, "OTP read failed\n"); \
			ret =  BCME_ERROR; \
			goto done; \
		} \
		if ((wlu_update_crc(wl, tmp_buf, 0, &otpcrc_config)) != BCME_OK) { \
			fprintf(stderr, "CRC update failed\n"); \
			ret =  BCME_ERROR; \
			goto done; \
		} \
	} \

/* In a corner case there is a possibility that data can be corrupted
* between writing it to OTP and reading it back from OTP.
* So to ensure there is no bit flips, compare the read back data.
*/
#define OTPREAD_COMPARE_SETCRC(offset1, ptr2, offset2, length, prevu) \
	if (integrity_chk) { \
		if (prevu) { \
			memcpy((tmp_buf + offset1), (ptr2 + offset2), length); \
		} else { \
			if (otpread_and_compare(wl, tmp_buf, offset1, \
				ptr2, offset2, length) != BCME_OK) { \
				ret = BCME_ERROR; \
				goto done; \
			} \
		} \
		ret = wlu_update_crc(wl, tmp_buf, prevu, &otpcrc_config); \
	} \

static cmd_t wl_otp_cmds[] = {
	{ "otpraw", wl_otpraw, WLC_GET_VAR, WLC_SET_VAR,
	"Read/Write raw data to on-chip otp\n"
	"Usage: wl otpraw <offset> <bits> [data]\n"
	"\toffset -bit offset where data to be read/write\n"
	"\tbits - Number of bits to be read/write\n"
	"\tdata - data stream in hex. '0x' before data stream is optional\n"},
	{ "otpw", wl_otpw, -1, WLC_OTPW,
	"Write an srom image to on-chip otp\n"
	"Usage: wl otpw <file>\n"},
	{ "nvotpw", wl_otpw, -1, WLC_NVOTPW,
	"Write nvram to on-chip otp\n"
	"Usage: wl nvotpw file"},
	{ "ciswrite", wlu_ciswrite, -1, WLC_SET_VAR,
	"Write specified <file> to the SDIO/PCIe CIS source (either SROM or OTP)\n"
	"\tUsage: ciswrite [-p|--pciecis] <file> [--preview]\n"
	"\tArguments can be provided in any sequence\n"
	"\t-p|--pciecis - Write OTP for PCIe full-dongle\n"
	"\t--preview - option allows you to review the update without committing it\n"},
	{ "cisupdate", wlu_cisupdate, -1, WLC_SET_VAR,
	"Write a hex byte stream to specified byte offset to the CIS source (either SROM or OTP)\n"
	"\tUsage: cisupdate <byte offset> <hex byte stream> [--preview]\n"
	"\t--preview - option allows you to review the update without committing it\n"},
	{ "cisdump", wlu_cisdump, WLC_GET_VAR, -1,
	"Display the content of the SDIO CIS source\n"
	"\t-b <file> -- also write raw bytes to <file>\n"
	"\t<len> -- optional count of bytes to display (must be even)"},
	{ "otpcrc", wlu_otpcrc, WLC_GET_VAR, -1,
	"Check if currently programmed CRC is correct or not.\n"
	"\tUsage: otpcrc [--set]\n"
	"\t--set - Update CRC if currently programmed CRC is wrong\n"},
	{ "otpcrcconfig", wlu_otpcrcconfig, WLC_GET_VAR, -1,
	"Get/Set the crc configuration at configuration space on OTP.\n"
	"\tUsage: otpcrcconfig [config] [-a <addr> -s <size> [-v] <ver>]\n"
	"\tArguments can be provided in any sequence\n"
	"\tEither hex value of config or individual parameters are allowed\n"
	"\t-a <addr> - End address of OTP CRC region\n"
	"\t-s <size> - Size of OTP CRC region\n"
	"\t-v <ver> - Version of OTP CRC configuration (optional)\n"},
	{ "otpdump", wl_otpdump_iter, WLC_GET_VAR, -1,
	"Dump raw otp"},
	{ "otpstat", wl_var_setintandprintstr, WLC_GET_VAR, -1,
	"Dump OTP status"},
	{ "otpecc_rows", wl_otpecc_rows, WLC_GET_VAR, WLC_SET_VAR,
	"1. cmdtype '0' : Dump raw otp and ecc status by rows"
	"2. cmdtype '1' : enable ECC and generate Parity per row to on-chip otp"
	"Usage: wl otpecc_rows <cmdtype> <rowoffset> <numrows>"},
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

void
wluc_otp_module_init()
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register bmac commands */
	wl_module_cmds_register(wl_otp_cmds);
}

uint8 *
wlu_get_otp_read_buf(void *wl, uint32 *size)
{
	int otpsize;

	if (wlu_iovar_getint(wl, "otpsize", &otpsize) != BCME_OK) {
		return NULL;
	}
	*size = otpsize = dtoh32(otpsize);
	if (otpsize > 0) {
		return malloc(otpsize);
	} else {
		return NULL;
	}
}

/*
 * The buffer should be big enough to accomodate the nbytes of data.
 * nbytes should be 0 to read the full OTP.
 */
int
wlu_read_otp_data(void *wl, uint8 *buf, int nbytes)
{
	int ret;
	int otpsize;
	wl_otpread_cmd_t read_cmd;

	if ((ret = wlu_iovar_getint(wl, "otpsize", &otpsize)) != BCME_OK) {
		return BCME_ERROR;
	}

	if (nbytes > otpsize) {
		return BCME_ERROR;
	}

	read_cmd.version = WL_OTPREAD_VER;
	read_cmd.cmd_len = sizeof(read_cmd);
	read_cmd.rdmode = 0;
	read_cmd.rdsize = nbytes ? nbytes : otpsize;
	read_cmd.rdoffset = 0;

	ret = wlu_iovar_getbuf(wl, "otpread", &read_cmd, sizeof(read_cmd),
			buf, otpsize);
	return ret;
}

int8
wlu_get_crc_config(void *wl, otpcrc_config_t *otpcrc_config)
{
	int ret;
	int otpsize;
	wl_otpread_cmd_t read_cmd;
	uint8 tmp_buf[32];
	uint16 crc_config;

	if ((ret = wlu_iovar_getint(wl, "otpsize", &otpsize)) != BCME_OK) {
		printf("OTP size read failed ret:%d\n", ret);
		return BCME_BAD_CRCCONFIG;
	}

	otpsize = dtoh32(otpsize);

	read_cmd.version = WL_OTPREAD_VER;
	read_cmd.cmd_len = sizeof(read_cmd);
	read_cmd.rdmode = 0;
	read_cmd.rdsize = htod32(sizeof(uint16));
	read_cmd.rdoffset = htod32(otpsize - sizeof(uint16));

	if ((ret = wlu_iovar_getbuf(wl, "otpread", &read_cmd, sizeof(read_cmd),
			tmp_buf, sizeof(tmp_buf))) == BCME_OK) {
		crc_config = *((uint16 *)(tmp_buf));

		if ((crc_config == 0x0000) || (crc_config == 0xFFFF)) {
			return BCME_NO_CRCCONFIG;
		}

		otpcrc_config->crc_ver = (crc_config & OTPCRC_CRC_VER_MASK) >> OTPCRC_CRC_VER_SHIFT;
		otpcrc_config->end_addr = crc_config & OTPCRC_END_ADDR_MASK;
		/* CRC block size is calculated as  4* value stored in bits 13:11 of crc config */
		otpcrc_config->num_crc = OTPCRC_REGION_SIZE_MULT  *
			((crc_config & OTPCRC_NUM_CRC_MASK) >>
			OTPCRC_NUM_CRC_SHIFT);
		if ((otpcrc_config->end_addr == 0) ||
				(otpcrc_config->end_addr > otpsize) ||
				(otpcrc_config->crc_ver == 0) ||
				(otpcrc_config->num_crc == 0))  {
			fprintf(stderr, "CRC info 0x%04X not valid\n", crc_config);
			return BCME_BAD_CRCCONFIG;
		} else {
			return BCME_VALID_CRCCONFIG;
		}
	} else {
		printf("OTP read failed ret:%d\n", ret);
		return BCME_BAD_CRCCONFIG;
	}
}

static uint8
wlu_get_last_crc(uint8 *buf, otpcrc_config_t *otpcrc_config)
{
	int i;

	/* If noting is written at the last address of CRC region then
	  * probably this is the first time we are trying to write the OTP.
	  */
	if (buf[otpcrc_config->end_addr] == 0x00) {
		return 0x00;
	}

	for (i = 1; i < otpcrc_config->num_crc; i++) {
		if (buf[otpcrc_config->end_addr - i] == 0x00) {
			return buf[(otpcrc_config->end_addr - i) + 1];
		}
	}

	/* This will return the CRC stored at last memory
	  * (CRC region start) location in the OTP CRC region
	  */
	return buf[otpcrc_config->end_addr - i + 1];
}

static uint8
wlu_is_crc_mem_avail(uint8 *buf, otpcrc_config_t *otpcrc_config)
{
	int i;

	/* If noting is written at the last address of CRC region then
	  * probably this is the first time we are trying to write the OTP.
	  */
	if (buf[otpcrc_config->end_addr] == 0x00) {
		return TRUE;
	}

	for (i = 1; i < otpcrc_config->num_crc; i++) {
		if (buf[otpcrc_config->end_addr - i] == 0x00) {
			return TRUE;
		}
	}

	return FALSE;
}

int8
wlu_update_crc(void *wl, uint8 *buf, uint32 preview, otpcrc_config_t *otpcrc_config)
{
	int i;
	uint8 crc, last_crc;
	otp_raw_t otp_crc;
	uint8 ret = BCME_ERROR;
	uint32 *crc_base_addr;

	/* Calculate CRC from 0x00 to (CRC_REGN start address - 1)  with int value 0xFF */
	crc = ~(otp_crc8((uint8 *)buf, (otpcrc_config->end_addr -
			otpcrc_config->num_crc) + 1, CRC8_INIT_VALUE));
	if (crc == 0x00) {
		/* If CRC is 0x00, we store it as 0xFF */
		crc = 0xFF;
	}

	last_crc = wlu_get_last_crc(buf, otpcrc_config);
	if (crc == last_crc) {
		if (!preview) {
			fprintf(stdout, "Same CRC, No data change\n");
			return BCME_OK;
		} else {
			ret = BCME_OK;
			goto do_preview;
		}
	}

	for (i = 0; i < otpcrc_config->num_crc; i++) {
		if (buf[otpcrc_config->end_addr - i] == 0x00) {
			/* Write CRC in OTP */
			buf[otpcrc_config->end_addr - i] = crc;
			crc_base_addr = (uint32 *)&buf[(otpcrc_config->end_addr - i) &
					OTPCRC_ALIGN_4_BIT_MASK];
			otp_crc.bit_offset = htod32(((otpcrc_config->end_addr - i) *
					OTPCRC_BITS_IN_BYTE) &
					OTPCRC_ALIGN_32_BIT_MASK);
			otp_crc.num_bits = htod32(OTPCRC_CRC_WRITE_SIZE);
			otp_crc.crc = htod32(*crc_base_addr);
			fprintf(stdout, "CRC:0x%02X @ 0x%04X\n",
				crc, (otpcrc_config->end_addr - i));
			ret = BCME_OK;
			break;
		}
	}
do_preview:
	if (ret == BCME_OK) {
		if (preview) {
			int otpsize, j;

			if ((ret = wlu_iovar_getint(wl, "otpsize", &otpsize)) != BCME_OK) {
				fprintf(stdout, "OTP size read failed\n");
				return ret;
			}
			/* Skip MFG region */
			buf += OTPCRC_MFG_REGION_SIZE;
			for (j = 0; j < (otpsize - OTPCRC_MFG_REGION_SIZE); j++) {
				if ((j % 8) == 0)
					printf("\nByte 0x%03X: ", j);
				fprintf(stdout, "0x%02x ", (uint8)buf[j]);
			}
			fprintf(stdout, "\n");

		} else {
			ret = wlu_var_setbuf(wl, "otpraw", &otp_crc, sizeof(otp_crc));
		}
	}

	return ret;
}

/* CRC will be calculated starting from address 0 of OTP till end of User region. */
int
wlu_check_otp_integrity(uint8 *buf, otpcrc_config_t *otpcrc_config)
{
	uint8 crc;
	uint8 last_crc;

	last_crc = wlu_get_last_crc(buf, otpcrc_config);

	/* If last CRC is 0x00, it means we did not stored CRC till now and this is the first time
	  * we are trying to read the CRC before any write.
	  */
	if (last_crc == 0x00) {
		return BCME_ERROR;
	}

	crc = ~(otp_crc8(buf, (otpcrc_config->end_addr - otpcrc_config->num_crc) + 1,
		CRC8_INIT_VALUE));
	if (crc == 0x00) {
		/* If CRC is 0x00, we store it as 0xFF */
		crc = 0xFF;
	}

	if (crc == last_crc) {
		return BCME_OK;
	} else {
		return BCME_ERROR;
	}
}

static int8
wlu_otpcrc_validate_otp(void *wl, uint8 **tmp_buf, otpcrc_config_t *otpcrc_config,
		uint32 *otpsize)
{
	int8 ret = BCME_OK;

	if ((*tmp_buf = wlu_get_otp_read_buf(wl, otpsize)) == NULL) {
		return BCME_NOMEM;
	}
	memset(*tmp_buf, 0, *otpsize);
	/* Read full OTP for integrity check */
	if (wlu_read_otp_data(wl, *tmp_buf, 0) != BCME_OK) {
		fprintf(stderr, "OTP read failed\n");
		ret = BCME_ERROR;
		goto done;
	}
	if (wlu_check_otp_integrity(*tmp_buf, otpcrc_config) != BCME_OK) {
		fprintf(stderr, "OTP CRC check failed\n");
		ret = BCME_ERROR;
		goto done;
	}
	return BCME_OK;
done:
	if (*tmp_buf) {
		free(*tmp_buf);
	}
	return ret;
}

static int8 otpread_and_compare(void *wl, uint8 *buf1, uint32 offset1,
		uint8 *buf2, uint32 offset2, uint32 len)
{
	uint32 i;
	if (wlu_read_otp_data(wl, buf1, 0) != BCME_OK) {
		fprintf(stderr, "OTP read failed\n");
		return BCME_ERROR;
	}
	for (i = 0; i < len; i++) {
		if ((buf1[i + offset1]) != (buf2[i+ offset2])) {
			fprintf(stderr, "comp fail O1:0x%02X O2:0x%02X D1:0x%02X D2:0x%02X\n",
				(offset1 + i), (offset2 + i), buf1[i + offset1], buf2[i + offset2]);
			return BCME_ERROR;
		}
	}
	return BCME_OK;
}

int
wlu_ciswrite(void *wl, cmd_t *cmd, char **argv)
{
#if !defined(BWL_FILESYSTEM_SUPPORT)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return (-1);
#elif	defined(_CFE_)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return CFE_ERR_UNSUPPORTED;
#elif	defined(DONGLEBUILD)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return 0;
#else
	char *arg, *bufp;
	FILE *fp = NULL;
	int ret = 0;
	uint32 len;
	uint8 integrity_chk = 0;
	uint8 *tmp_buf = NULL;
	otpcrc_config_t otpcrc_config;
	uint32 otpsize;

	cis_rw_t cish;
	char *cisp, *cisdata;
	int preview = 0;

	UNUSED_PARAMETER(cmd);

	/* arg check -- error if no arg */
	if (!*++argv)
		return BCME_USAGE_ERROR;

	memset((char*)&cish, 0, sizeof(cish));
	if ((!strcmp(*argv, "--pciecis")) || (!strcmp(*argv, "-p"))) {
		cish.flags |= CISH_FLAG_PCIECIS;	/* write CIS format bit */
		if (argv[1] == NULL) {
			return BCME_USAGE_ERROR;
		}
		argv++;
	}
	arg = *argv;
	argv++;

	while (*argv) {
		if (!strcmp(*argv, "--preview")) {
			preview = 1;
			argv++;
			continue;
		}
		/* If arguments do not match with any of the above, its bad arg */
		return BCME_BADARG;
	}

	OTPCRC_VALIDATE_INTEGRITY

	/* initialize buffer with iovar */
	bufp = buf;
	memset(buf, 0, WLC_IOCTL_MAXLEN);
	strcpy(bufp, "ciswrite");
	bufp += strlen("ciswrite") + 1;
	cisp = bufp;
	cisdata = cisp + sizeof(cish);

	if (!(fp = fopen(arg, "rb"))) {
		fprintf(stderr, "%s: No such file or directory\n", arg);
		ret = BCME_BADARG;
		goto done;
	}

	len = fread(cisdata, 1, SROM_MAX + 1, fp);
	if ((ret = ferror(fp))) {
		printf("\nerror %d reading %s\n", ret, arg);
		ret = BCME_ERROR;
		goto done;
	}

	if (!feof(fp)) {
		printf("\nFile %s is too large\n", arg);
		ret = BCME_ERROR;
		goto done;
	}

	/* Convert the endianess for non-zero fields */
	cish.flags = htod16(cish.flags);
	cish.nbytes = htod32(len); /* fill in length (offset is 0) */
	memcpy(cisp, (char*)&cish, sizeof(cish));

	printf("len %d sizeof(cish) %d total %d\n", len, (int)sizeof(cish),
	       (int)(len + sizeof(cish)));

	OTPCRC_CHECK_CRC_MEM_AVAILABILITY

	if (preview) {
		if (!integrity_chk) {
			uint32 i;
			for (i = 0; i < len; i++) {
				if ((i % 8) == 0)
					printf("\nByte 0x%03X: ", i);
				printf("0x%02x ", (uint8)cisdata[i]);
			}
			printf("\n");
		}

	} else {
		ret = wl_set(wl, WLC_SET_VAR, buf, (cisp - buf) + sizeof(cish) + len);
		if (ret < 0) {
			fprintf(stderr, "ciswrite failed: %d\n", ret);
			goto done;
		}
	}

	OTPREAD_COMPARE_SETCRC(OTPCRC_MFG_REGION_SIZE, ((uint8 *)cisdata), 0, len, preview)
done:
	if (fp)
		fclose(fp);
	if (tmp_buf) {
		free(tmp_buf);
	}

	return ret;
#endif   /* BWL_FILESYSTEM_SUPPORT */
}

int
wlu_cisupdate(void *wl, cmd_t *cmd, char **argv)
{
#if	defined(_CFE_)
	return CFE_ERR_UNSUPPORTED;
#elif	defined(DONGLEBUILD)
	return 0;
#else
	char *bufp, *endptr;
	int ret = 0;
	int preview = 0;
	uint32 off;
	uint32 len;
	uint32 updatelen;
	uint32 i;
	char hexstr[3];
	char bytes[SROM_MAX];
	uint8 integrity_chk = 0;
	uint8 *tmp_buf = NULL;
	otpcrc_config_t otpcrc_config;
	uint32 otpsize;
	cis_rw_t cish;
	char *cisp;

	UNUSED_PARAMETER(cmd);

	/* validate arg count */
	if (!*++argv || !argv[1])
		return BCME_USAGE_ERROR;

	/* grab byte offset */
	off = (uint32)strtol(argv[0], &endptr, 0);
	if (*endptr != '\0')
		return BCME_USAGE_ERROR;

	bufp = argv[1];
	printf("offset %d data %s\n", off, argv[1]);

	/* Consume both used args */
	argv += 2;

	while (*argv) {
		if (!strcmp(*argv, "--preview")) {
			preview = 1;
			argv++;
			continue;
		}
		/* If arguments do not match with any of the above, its bad arg */
		return BCME_BADARG;
	}

	updatelen = strlen(bufp);
	if (updatelen % 2) {
		fprintf(stderr, "cisupdate hex string must contain an even number of digits\n");
		goto done;
	}
	updatelen /= 2;

	/* convert and store hex byte values */
	for (i = 0; i < updatelen; i++) {
		hexstr[0] = *bufp;
		hexstr[1] = *(bufp + 1);
		if (!isxdigit((int)hexstr[0]) || !isxdigit((int)hexstr[1])) {
			fprintf(stderr, "cisupdate invalid hex digit(s) in %c%c\n",
				hexstr[0], hexstr[1]);
			goto done;
		}
		hexstr[2] = '\0';
		bytes[i] = (char) strtol(hexstr, NULL, 16);
		bufp += 2;
	}

	/* Prepare the read info */
	memset((char*)&cish, 0, sizeof(cish));

	/* set up the buffer and do the get (+9 allows space for "ciswrite" string later) */
	memset(buf + 9, 0, (WLC_IOCTL_MAXLEN - 9));
	strcpy(buf + 9, "cisdump");
	bufp = buf + strlen("cisdump") + 1 + 9;
	memcpy(bufp, (char*)&cish, sizeof(cish));
	bufp += sizeof(cish);
	ret = wl_get(wl, WLC_GET_VAR, buf + 9, (bufp - (buf + 9)) + SROM_MAX);
	if (ret < 0) {
		fprintf(stderr, "cisupdate failed to read cis: %d\n", ret);
		goto done;
	}

	/* pull off the cis_rw_t */
	bufp = buf + 9;
	memcpy((char*)&cish, bufp, sizeof(cish));
	len = dtoh32(cish.nbytes);

	if ((off + updatelen) > len) {
		fprintf(stderr, "cisupdate offset %d plus update len %d exceeds CIS len %d\n",
		        off, updatelen, len);
		goto done;
	}

	/* move past to the data */
	bufp += sizeof(cish);

	OTPCRC_VALIDATE_INTEGRITY

	/* update the bytes */
	if (dtoh16(cish.source) == WLC_CIS_SROM) {
		for (i = 0; i < updatelen; ++i)
			bufp[off + i] = bytes[i] & 0xff;
	} else {
		for (i = 0; i < updatelen; ++i) {
			if (~bytes[i] & bufp[off + i]) {
				fprintf(stderr, "cisupdate: OTP update incompatible:"
				        " update[%d](0x%02x)->cis[%d](0x%02x)\n",
				        i,  bytes[i], off + i, bufp[off + i]);
				goto done;
			}
			bufp[off + i] |= bytes[i];
		}
	}

	/* initialize buffer with iovar */
	bufp = buf;
	strcpy(bufp, "ciswrite");
	bufp += strlen("ciswrite") + 1;
	cisp = bufp;

	/* fill in cis_rw_t fields */
	memset((char*)&cish, 0, sizeof(cish));
	cish.nbytes = htod32(len);
	memcpy(cisp, (char*)&cish, sizeof(cish));

	OTPCRC_CHECK_CRC_MEM_AVAILABILITY

	/* write the data back to the device */
	if (preview) {
		if (!integrity_chk) {
			bufp += sizeof(cish);
			for (i = 0; i < len; i++) {
				if ((i % 8) == 0)
					printf("\nByte 0x%03X: ", i);
				printf("0x%02x ", (uint8)bufp[i]);
			}
			printf("\n");
		}
	} else {
		ret = wl_set(wl, WLC_SET_VAR, buf, (cisp - buf) + sizeof(cish) + len);
		if (ret < 0) {
			fprintf(stderr, "cisupdate cis write failed: %d\n", ret);
			goto done;
		}
	}
	if (integrity_chk) {
		memcpy((tmp_buf + OTPCRC_MFG_REGION_SIZE), (cisp + sizeof(cish)), len);
		if ((ret = wlu_update_crc(wl, tmp_buf, preview, &otpcrc_config)) != BCME_OK) {
			fprintf(stderr, "CRC update failed\n");
		}
	}

done:
	if (tmp_buf)
		free(tmp_buf);
	return ret;
#endif /* _CFE_ */
}

static int
wlu_otpcrc(void *wl, cmd_t *cmd, char **argv)
{
	uint8 *tmp_buf = NULL;
	otpcrc_config_t otpcrc_config;
	uint8 update_crc = 0;
	uint32 otpsize;
	int8 ret = BCME_OK;

	UNUSED_PARAMETER(cmd);

	if (argv[1]) {
		if (!strcmp(argv[1], "--set")) {
			update_crc = 1;
		} else {
			fprintf(stderr, "Bad arg %s\n", argv[1]);
			return BCME_USAGE_ERROR;
		}
	}

	if ((ret = wlu_get_crc_config(wl, &otpcrc_config)) == BCME_NO_CRCCONFIG) {
		fprintf(stdout, "OTP_CRC_Config not programmed\n");
		return  BCME_OK;
	} else {
		if (ret != BCME_VALID_CRCCONFIG) {
			return  BCME_ERROR;
		}
	}

	/* Whenever control reaches here the value of ret will always be
	 * BCME_VALID_CRCCONFIG which is equivalent to BCME_OK.
	 */
	if ((tmp_buf = wlu_get_otp_read_buf(wl, &otpsize)) == NULL) {
		ret = BCME_NOMEM;
		goto done;
	}
	memset(tmp_buf, 0, otpsize);
	if (wlu_read_otp_data(wl, tmp_buf, 0) != BCME_OK) {
		fprintf(stderr, "OTP read failed\n");
		ret =  BCME_ERROR;
		goto done;
	}
	if (wlu_check_otp_integrity(tmp_buf, &otpcrc_config) != BCME_OK) {
		fprintf(stderr, "CRC check failed\n");
		if (update_crc) {
			fprintf(stdout, "Updating CRC ... \n");
			if ((wlu_update_crc(wl, tmp_buf, 0, &otpcrc_config)) != BCME_OK) {
				fprintf(stderr, "CRC update failed\n");
				ret = BCME_ERROR;
				goto done;
			}
		}
	} else {
		fprintf(stdout, "CRC check Pass\n");
	}

done:
	if (tmp_buf)
		free(tmp_buf);
	return ret;
}

int
wlu_otpcrcconfig(void *wl, cmd_t *cmd, char **argv)
{
	otpcrc_config_t otpcrc_config;
	uint16 crc_config = 0;
	char *endptr;
	uchar set = 0;
	otp_raw_t otp_crc;
	uint8 *tmp_buf = NULL;
	uint32 otpsize;
	int8 ret = BCME_OK;
	uint16 *config_addr;

	UNUSED_PARAMETER(cmd);

	argv++;

	if (*argv) {
		crc_config = (uint16)strtoul(argv[0], &endptr, 0);
		if (*endptr == '\0') {
			otpcrc_config.crc_ver = (crc_config & OTPCRC_CRC_VER_MASK) >>
				OTPCRC_CRC_VER_SHIFT;
			otpcrc_config.end_addr = crc_config & OTPCRC_END_ADDR_MASK;
			/* CRC block size is calculated as
			 * 4* value stored in bits 13:11 of crc config
			 */
			otpcrc_config.num_crc = OTPCRC_REGION_SIZE_MULT  *
				((crc_config & OTPCRC_NUM_CRC_MASK) >>
				OTPCRC_NUM_CRC_SHIFT);
			if (argv[1] != NULL) {
				return BCME_BADARG;
			} else {
				goto validate_config;
			}
		}
		memset(&otpcrc_config, 0, sizeof(otpcrc_config));
		while (*argv) {
			if (!strcmp(argv[0], "-v")) {
				if (argv[1] != NULL) {
					otpcrc_config.crc_ver =
						(uint16)strtoul(argv[1], &endptr, 0);
					if (*endptr != '\0') {
						fprintf(stderr, "Invalid ver %s\n", argv[1]);
						return BCME_BADARG;
					}
					if ((otpcrc_config.crc_ver == 0) ||
						(otpcrc_config.crc_ver > OTPCRC_VER_MAX)) {
						fprintf(stderr, "Invalid Version. 0 < ver <= 3\n");
						return BCME_BADARG;
					}
				} else {
					fprintf(stderr, "No version arg\n");
					return BCME_USAGE_ERROR;

				}
			}

			if (!strcmp(argv[0], "-s")) {
				if (argv[1] != NULL) {
					otpcrc_config.num_crc =
						(uint16)strtoul(argv[1], &endptr, 0);
					if (*endptr != '\0') {
						fprintf(stderr, "Invalid size %s\n", argv[1]);
						return BCME_BADARG;
					}
					if ((otpcrc_config.num_crc > OTPCRC_REGION_SIZE_MAX) ||
						(otpcrc_config.num_crc == 0) ||
						(otpcrc_config.num_crc %
							OTPCRC_REGION_SIZE_MULT))  {
						fprintf(stderr, "Invalid crc region size %d\n",
							otpcrc_config.num_crc);
						fprintf(stderr, "Size should be <= 28"
							" and multiple of %d\n",
							OTPCRC_REGION_SIZE_MULT);
						return BCME_USAGE_ERROR;
					}
				} else {
					fprintf(stderr, "No size arg\n");
					return BCME_USAGE_ERROR;

				}
			}

			if (!strcmp(argv[0], "-a")) {
				if (argv[1] != NULL) {
					otpcrc_config.end_addr =
						(uint16)strtoul(argv[1], &endptr, 0);
					if (*endptr != '\0') {
						fprintf(stderr, "Invalid ver %s\n", argv[1]);
						return BCME_BADARG;
					}
					if (otpcrc_config.end_addr & (~OTPCRC_END_ADDR_MASK)) {
						fprintf(stderr, "Invalid addr 0x%X\n",
							otpcrc_config.end_addr);
						return BCME_BADARG;
					}
				} else {
					fprintf(stderr, "No address arg\n");
					return BCME_USAGE_ERROR;

				}
			}
			argv += 2;
		}
		if (otpcrc_config.crc_ver == 0) {
			otpcrc_config.crc_ver = 1;
		}
validate_config:
		/* There should be a check to confirm if the OTP CRC region addr
		 * falls under SW user region
		 */
		if ((otpcrc_config.crc_ver == 0) ||
			(otpcrc_config.end_addr == 0) ||
			(otpcrc_config.num_crc == 0) ||
			(((otpcrc_config.end_addr - otpcrc_config.num_crc) + 1) <
			OTPCRC_MFG_REGION_SIZE)) {
			fprintf(stderr, "Invalid/Insufficient args\n");
			return BCME_USAGE_ERROR;

		}
		set = 1;
		crc_config = otpcrc_config.end_addr |
			(otpcrc_config.num_crc /OTPCRC_REGION_SIZE_MULT)  << OTPCRC_NUM_CRC_SHIFT |
			(otpcrc_config.crc_ver << OTPCRC_CRC_VER_SHIFT);
	}

	if (set) {
		if ((tmp_buf = wlu_get_otp_read_buf(wl, &otpsize)) == NULL) {
			return BCME_NOMEM;
		}
		if (wlu_read_otp_data(wl, tmp_buf, 0) != BCME_OK) {
			fprintf(stderr, "OTP read failed\n");
			ret = BCME_ERROR;
			goto done;
		}

		config_addr = (uint16 *)(tmp_buf +otpsize - sizeof(crc_config));
		/* Check new crc config compatibility with current written crc config if any */
		if (~crc_config & *config_addr) {
			fprintf(stderr, "new config: 0x%04X not compatible to old:0x%04X\n",
				crc_config, *((uint16 *)(tmp_buf +otpsize - sizeof(crc_config))));
			ret = BCME_BADARG;
			goto done;
		}

		/* Update new config in buffer */
		*config_addr = crc_config;

		/* Each OTP raw write is 32 bit alligned i.e. 4 byte alligned */
		otp_crc.bit_offset = htod32(((otpsize - sizeof(uint32)) * OTPCRC_BITS_IN_BYTE) &
				OTPCRC_ALIGN_32_BIT_MASK);
		otp_crc.num_bits = htod32(OTPCRC_CRC_WRITE_SIZE);
		otp_crc.crc = htod32(*((uint32 *)(tmp_buf + otpsize - sizeof(uint32))));

		printf("Setting config 0x%X\n", crc_config);
		if ((ret = wlu_var_setbuf(wl, "otpraw",
				&otp_crc, sizeof(otp_crc))) != BCME_OK) {
			fprintf(stderr, "Config write failed %d\n", ret);
			ret =  BCME_ERROR;
			goto done;
		}

		/* If we are trying to write 0xFFFF or over-rideing previous crc config
		 * then we should not update the CRC.
		 */
		if (crc_config == 0xFFFF) {
			goto done;
		}

		if ((wlu_update_crc(wl, tmp_buf, 0, &otpcrc_config)) != BCME_OK) {
			fprintf(stderr, "CRC update failed\n");
			ret =  BCME_ERROR;
			goto done;
		}
	} else {
		if ((ret = wlu_get_crc_config(wl, &otpcrc_config)) == BCME_NO_CRCCONFIG) {
			fprintf(stdout, "OTP_CRC_Config not programmed\n");
			ret = BCME_OK;
			goto done;
		} else {
			if (ret == BCME_VALID_CRCCONFIG) {
				printf("CRC info Ver: \t%d\n", otpcrc_config.crc_ver);
				printf("CRC block size:\t%d\n", otpcrc_config.num_crc);
				printf("Start Addr: \t%d(0x%04X)\n",
					(otpcrc_config.end_addr - otpcrc_config.num_crc + 1),
					(otpcrc_config.end_addr - otpcrc_config.num_crc + 1));
				printf("End Addr: \t%d(0x%04X)\n", otpcrc_config.end_addr,
					otpcrc_config.end_addr);
			}
		}
	}
done:
	if (tmp_buf)
		free(tmp_buf);
	return ret;
}

static int
wlu_cisdump(void *wl, cmd_t *cmd, char **argv)
{
	char *bufp;
	int i, ret = 0;
	cis_rw_t cish;
	uint nbytes = 0;
	char *fname = NULL;

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(fname);

	/* Grab and move past optional output file argument */
	if ((argv[1] != NULL) && (strcmp(argv[1], "-b") == 0)) {
		fname = argv[2];
		argv += 2;
	}

	/* check for a length argument */
	if (*++argv != NULL) {
		nbytes = (int)strtol(*argv, NULL, 0);
		if (nbytes & 1) {
			printf("Invalid byte count %d, must be even\n", nbytes);
			ret = BCME_BADARG;
			goto done;
		}
		if (nbytes > SROM_MAX) {
			printf("Count %d too large\n", nbytes);
			ret = BCME_BUFTOOLONG;
			goto done;
		}
	}

	/* Prepare the read info */
	memset((char*)&cish, 0, sizeof(cish));
	cish.nbytes = htod32(nbytes);

	/* set up the buffer and do the get */
	memset(buf, 0, WLC_IOCTL_MAXLEN);
	strcpy(buf, "cisdump");
	bufp = buf + strlen("cisdump") + 1;
	memcpy(bufp, (char*)&cish, sizeof(cish));
	bufp += sizeof(cish);
	ret = wl_get(wl, WLC_GET_VAR, buf, (bufp - buf) + (nbytes ? nbytes : SROM_MAX));
	if (ret < 0) {
		fprintf(stderr, "Failed cisdump request: %d\n", ret);
		goto done;
	}

	/* pull off the cis_rw_t */
	bufp = buf;
	memcpy((char*)&cish, bufp, sizeof(cish));
	cish.source = dtoh16(cish.source);
	cish.flags = dtoh16(cish.flags);
	cish.byteoff = dtoh32(cish.byteoff);
	cish.nbytes = dtoh32(cish.nbytes);

	/* move past to the data */
	bufp += sizeof(cish);

	printf("Source: %d (%s)", cish.source,
	       (cish.source == WLC_CIS_DEFAULT) ? "Built-in default" :
	       (cish.source == WLC_CIS_SROM) ? "External SPROM" :
	       (cish.source == WLC_CIS_OTP) ? "Internal OTP" : "Unknown?");
	if (!nbytes)
		printf("\nMaximum length: %d bytes", cish.nbytes);
	for (i = 0; i < (int)cish.nbytes; i++) {
		if ((i % 8) == 0)
			printf("\nByte 0x%3X: ", i);
		printf("0x%02x ", (uint8)bufp[i]);
	}
	printf("\n");

#if defined(BWL_FILESYSTEM_SUPPORT)
#if !defined(_CFE_) && !defined(DONGLEBUILD)
	if (fname != NULL) {
		FILE *fp;

		if (!nbytes)
			nbytes = cish.nbytes;

		fp = fopen(fname, "wb");
		if (fp != NULL) {
			ret = fwrite(bufp, 1, nbytes, fp);
			if (ret != (int)nbytes) {
				fprintf(stderr, "Error writing %d bytes to file, rc %d!\n",
				        (int)nbytes, ret);
				ret = BCME_ERROR;
			} else {
				printf("Wrote %d bytes to %s\n", ret, fname);
				ret = 0;
			}
			fclose(fp);
		} else {
			fprintf(stderr, "Problem opening file %s\n", fname);
			ret = BCME_BADARG;
		}
	}
#endif /* !(CFE|DONGLEBUILD) -- has stdio filesystem */
#endif   /* BWL_FILESYSTEM_SUPPORT */

done:
	return ret;
}

int
wl_otpraw(void *wl, cmd_t *cmd, char **argv)
{
	char var[392];
	uint32 offset;
	uint32 bits;
	uint32 len;
	bool get = TRUE;
	void *ptr = NULL;
	char *endptr;
	uint32 i;
	uint8 integrity_chk = 0;
	uint8 *tmp_buf = NULL;
	otpcrc_config_t otpcrc_config;
	uint32 otpsize;
	int8 ret = BCME_OK;

	memset(var, 0, sizeof(var));
	if (argv[1]) {
		offset = htod32(strtoul(argv[1], &endptr, 0));
		memcpy(var, (char *)&offset, sizeof(offset));
		len = sizeof(offset);
	}
	else
		return BCME_USAGE_ERROR;

	if (argv[2]) {
		bits = htod32(strtoul(argv[2], &endptr, 0));
		if ((bits > 3072) || (bits == 0))
		{
			fprintf(stderr, "bit size (%d) too long or negative!!\n", bits);
			return BCME_BADARG;
		}
	}
	else
		return BCME_USAGE_ERROR;

	memcpy(&var[len], (char *)&bits, sizeof(bits));
	len += sizeof(bits);

	if (argv[3]) {
		unsigned char data[768];
		uint32  patlen;
		char *inptr = argv[3];

		get = FALSE;

		OTPCRC_VALIDATE_INTEGRITY

		if (*inptr == '0' && toupper((int)(*(inptr + 1))) == 'X')
			inptr += 2;

		patlen = strlen(inptr);
		if (patlen > 768 || (patlen * 4) < bits)
		{
			fprintf(stderr, "data length (%d) too long or small!!\n", patlen);
			ret = BCME_USAGE_ERROR;
			goto done;
		}

		for (i = 1; i <= patlen; i++)
		{
			int n = (int)((unsigned char)*inptr++);
			if (!isxdigit(n)) {
				fprintf(stderr, "invalid hex digit %c\n", n);
				ret = BCME_USAGE_ERROR;
				goto done;
			}
			data[patlen - i] = (unsigned char)(isdigit(n) ? (n - '0')
				        : ((islower(n) ? (toupper(n)) : n) - 'A' + 10));
		}

		for (i = 0; i < patlen; i += 2)
		{
			unsigned char v;
			v = data[i];
			if (i + 1 < patlen)
				v += (data[i+1] * 16);
			memcpy(&var[len], (char *)&v, sizeof(v));
			len += sizeof(v);
		}

		printf("OTP RAM Write:");
		for (i = 0; i < bits; i += 8)
		{
			unsigned char v;
			v = var[2*sizeof(uint32) + (i/8)];

			if ((i % 64) == 0)
				printf("\nbit %4d:", offset + i);
			printf(" 0x%x", v);
		}
		printf("\n");

	}

	if (get) {
		unsigned char v, *cptr;

		if ((ret = wlu_var_getbuf(wl, cmd->name, var, sizeof(var), &ptr)) < 0) {
			fprintf(stderr, "Error reading from OTP data\n");
			goto done;
		}

		cptr = (unsigned char *)ptr;

		printf("OTP RAM Read:");
		for (i = 0; i < bits; i += 8)
		{
			v = *cptr++;

			if ((i % 64) == 0)
				printf("\nbit %4d:", offset + i);
			printf(" 0x%02x", v);
		}
		printf("\n");
		ret = BCME_OK;
		goto done;
	}

	OTPCRC_CHECK_CRC_MEM_AVAILABILITY

	if (wlu_var_setbuf(wl, cmd->name, &var, sizeof(var)) != BCME_OK) {
		fprintf(stderr, "OTP write failed\n");
		ret = BCME_ERROR;
		goto done;
	}

	OTPCRC_READ_AND_UPDATE_CRC
done:
	if (tmp_buf) {
		free(tmp_buf);
	}
	return  ret;
}

int
wl_otpw(void *wl, cmd_t *cmd, char **argv)
{
#if !defined(BWL_FILESYSTEM_SUPPORT)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return (-1);
#elif	defined(_CFE_)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return CFE_ERR_UNSUPPORTED;
#elif	defined(DONGLEBUILD)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return 0;
#else
	FILE *fp;
	int ret = 0;
	struct nvram_header *nvr;
	char *p, otpw_buf[OTPCRC_OTPW_BUF_SIZE];
	const char *msg;
	int len;
	uint8 integrity_chk = 0;
	uint8 *tmp_buf = NULL;
	otpcrc_config_t otpcrc_config;
	uint32 otpsize;

	if (!*++argv)
		return BCME_USAGE_ERROR;

	OTPCRC_VALIDATE_INTEGRITY

	if (!(fp = fopen(*argv, "rb"))) {
		fprintf(stderr, "%s: No such file or directory\n", *argv);
		ret = BCME_BADARG;
		goto done;
	}

	len = fread(otpw_buf, 1, sizeof(otpw_buf) - 1, fp);
	if ((ret = ferror(fp))) {
		printf("\nerror %d reading %s\n", ret, *argv);
		ret = BCME_ERROR;
		goto done;
	}
	if (!feof(fp)) {
		printf("\nFile %s too large\n", *argv);
		ret = BCME_ERROR;
		goto done;
	}

	if (len < (int)sizeof(*nvr)) {
		printf("File size too small\n");
		ret = BCME_ERROR;
		goto done;
	}
	/* Got the bits, do they look like the output of nvserial? */
	nvr = (struct nvram_header *)otpw_buf;
	if (nvr->magic == NVRAM_MAGIC) {
		if (cmd->set == WLC_OTPW) {
			printf("File %s looks like an nvserial file, use nvotpw\n", *argv);
			fflush(stdout);
			ret = BCME_ERROR;
			goto done;
		}
		len  = nvr->len - sizeof(*nvr);
		if (len <= 0) {
			printf("Invalid length (%d)\n", len);
			ret = BCME_ERROR;
			goto done;
		}
		if (len & 1) {
			otpw_buf[len++] = '\0';
		}
		p = (char *)(nvr + 1);
		msg = "nvserial";
	} else {
		if (cmd->set == WLC_NVOTPW) {
			printf("File %s is not an nvserial file\n", *argv);
			ret = BCME_ERROR;
			goto done;
		}
		if (len & 1) {
			printf("File %s has an odd length (%d)\n", *argv, len);
			ret = BCME_ERROR;
			goto done;
		}
		p = otpw_buf;
		msg = "raw";
	}

	printf("Writing %d bytes from %s file %s to otp ...\n", len, msg, *argv);
	fflush(stdout);

	OTPCRC_CHECK_CRC_MEM_AVAILABILITY

	if ((ret = wlu_set(wl, cmd->set, p, len)) < 0) {
		printf("\nError %d writing %s to otp\n", ret, *argv);
	}

	OTPREAD_COMPARE_SETCRC(0, (uint8 *)p, 0, len, 0)

done:
	if (fp)  {
		fclose(fp);
	}
	if (tmp_buf) {
		free(tmp_buf);
	}

	return ret;
#endif /* BWL_FILESYSTEM_SUPPORT */
}

#if WL_OTPREAD_VER != 1
#error "Update this code to handle the new version of wl_otpread_cmd_t !"
#endif // endif

static int
wl_otpdump_iter(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int otpsize, readsize, offset = 0;
	uint i;
	uint16 *outbuf = (uint16 *)buf;
	wl_otpread_cmd_t read_cmd;

	UNUSED_PARAMETER(cmd);

	if (!*argv) {
		printf("set: missing arguments\n");
		return BCME_USAGE_ERROR;
	}

	if ((ret = wlu_iovar_getint(wl, "otpsize", &otpsize)) < 0) {
		return wl_var_setintandprintstr(wl, cmd, argv);
	}

	readsize = otpsize = dtoh32(otpsize);
	printf("otpsize: %d\n", otpsize);

	read_cmd.version = WL_OTPREAD_VER;
	read_cmd.cmd_len = sizeof(read_cmd);
	read_cmd.rdmode = 0;

	while (readsize) {
		read_cmd.rdsize = (readsize <= WLC_IOCTL_MAXLEN) ? readsize : WLC_IOCTL_MAXLEN;
		read_cmd.rdoffset = offset;

		memset(buf, 0, WLC_IOCTL_MAXLEN);

		ret = wlu_iovar_getbuf(wl, "otpread", &read_cmd, sizeof(read_cmd),
				buf, WLC_IOCTL_MAXLEN);

		if (ret < 0)
			return ret;

		for (i = 0; i < (read_cmd.rdsize / 2); i++) {
			if ((i % 4) == 0) {
				printf("\n0x%04x:", 2 * i + offset);
			}
			printf(" 0x%04x", outbuf[i]);
		}

		readsize -= read_cmd.rdsize;
		offset += read_cmd.rdsize;
	}
	printf("\n\n");

	return (0);
}

#if WL_OTPECC_ROWS_VER != 1
#error "Update this code to handle the new version of wl_otpread_cmd_t !"
#endif // endif

/*
 * wl otpecc_rows <cmdtype> <rowoffset> <numrows>
 *  - cmdtype '0' : Dump raw otp and ecc status by rows"
 *  - cmdtype '1' : enable ECC and generate Parity per row to on-chip otp"
 */
static int
wl_otpecc_rows(void *wl, cmd_t *cmd, char **argv)
{
	char *endptr;
	uint8 cmdtype;

	if (!argv[WL_OTPECC_ARGIDX_CMDTYPE+1]) {
		printf("Wrong syntax : need cmdtype - 0: read row data, 1: ECC lock\n");
		return BCME_USAGE_ERROR;
	}

	cmdtype = htod32(strtoul(argv[WL_OTPECC_ARGIDX_CMDTYPE+1], &endptr, 0));
	if (cmdtype == WL_OTPECC_ROWS_CMD_READ) {
		return (wl_otpecc_rowsdump(wl, cmd, argv));
	} else if (cmdtype == WL_OTPECC_ROWS_CMD_LOCK) {
		return (wl_otpecc_rowslock(wl, cmd, argv));
	} else {
		printf("Wrong syntax : need cmdtype - 0: read row data, 1: ECC lock\n");
		return BCME_USAGE_ERROR;
	}
}

static int
wl_otpecc_rowslock(void *wl, cmd_t *cmd, char **argv)
{
	char *endptr;
	int ret = 0;
	wl_otpecc_rows_t otpecc_cmd;

	if (!argv[WL_OTPECC_ARGIDX_ROWOFFSET+1]) {
		printf("Wrong syntax : need rowsoffset\n");
		return BCME_USAGE_ERROR;
	}

	otpecc_cmd.version = WL_OTPECC_ROWS_VER;
	otpecc_cmd.cmdtype = WL_OTPECC_ROWS_CMD_LOCK;
	otpecc_cmd.len = sizeof(otpecc_cmd);
	otpecc_cmd.rowoffset = htod32(strtoul(argv[WL_OTPECC_ARGIDX_ROWOFFSET+1], &endptr, 0));

	if (argv[WL_OTPECC_ARGIDX_NUMROWS+1]) {
		otpecc_cmd.numrows = htod32(strtoul(argv[WL_OTPECC_ARGIDX_NUMROWS+1], &endptr, 0));
	} else {
		otpecc_cmd.numrows = 1;
	}

	ret = wlu_var_setbuf(wl, cmd->name, &otpecc_cmd, sizeof(otpecc_cmd));

	return ret;
}

/*
 * wl_otpecc_rowsdump is to show otp raw data per row and ecc status per row.
 * WL_IOVAR variables that require offset and size.
 * (if no offset and no size provided, read all row. if no size provided, read 1 row.)
 */
static int
wl_otpecc_rowsdump(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	uint i;
	char *endptr;
	int otpsize, numrows, rowoffset = 0;
	uint16 eccstatus;
	bool ecccorrstatus, eccdblerr, eccdedst, eccsecst;
	uint8 eccenable, eccdt;
	uint16 *outbuf = (uint16 *)buf;
	wl_otpecc_rows_t read_cmd;

	if ((ret = wlu_iovar_getint(wl, "otpsize", &otpsize)) < 0) {
		return ret;
	}

	otpsize = dtoh32(otpsize);
	printf("otpsize: %d\n", otpsize);

	if (!argv[WL_OTPECC_ARGIDX_ROWOFFSET+1]) {
		rowoffset = 0;
		numrows = (otpsize + 3) >> 2;
	} else if (!argv[WL_OTPECC_ARGIDX_NUMROWS+1]) {
		rowoffset = htod32(strtoul(argv[WL_OTPECC_ARGIDX_ROWOFFSET+1], &endptr, 0));
		numrows = 1;
	} else {
		rowoffset = htod32(strtoul(argv[WL_OTPECC_ARGIDX_ROWOFFSET+1], &endptr, 0));
		numrows = htod32(strtoul(argv[WL_OTPECC_ARGIDX_NUMROWS+1], &endptr, 0));
	}

	read_cmd.version = WL_OTPECC_ROWS_VER;
	read_cmd.cmdtype = WL_OTPECC_ROWS_CMD_READ;
	read_cmd.len = sizeof(read_cmd);

	while (numrows) {
		read_cmd.numrows =
			(numrows <= (WLC_IOCTL_MAXLEN / WL_ECCDUMP_ROW_SIZE_BYTE))?
			numrows : (WLC_IOCTL_MAXLEN / WL_ECCDUMP_ROW_SIZE_BYTE);
		read_cmd.rowoffset = rowoffset;

		memset(buf, 0, WLC_IOCTL_MAXLEN);

		ret = wlu_iovar_getbuf(wl, cmd->name, &read_cmd, sizeof(read_cmd),
				buf, WLC_IOCTL_MAXLEN);

		if (ret < 0) {
			return ret;
		}

		for (i = 0; i < read_cmd.numrows; i++) {
			printf("\n%3d:", i + rowoffset);

			printf(" 0x%4x", outbuf[i * WL_ECCDUMP_ROW_SIZE_WORD]);
			printf(" 0x%4x", outbuf[i * WL_ECCDUMP_ROW_SIZE_WORD + 1]);

			eccstatus = outbuf[i * WL_ECCDUMP_ROW_SIZE_WORD + 2];

			eccdt = (eccstatus >> OTP_ECC_DATA_SHIFT) & OTP_ECC_DATA_MASK;
			eccsecst = (eccstatus >> OTP_ECC_SEC_ST_SHIFT) & OTP_ECC_SEC_ST_MASK;
			eccdedst = (eccstatus >> OTP_ECC_DED_ST_SHIFT) & OTP_ECC_DED_ST_MASK;
			eccdblerr = (eccstatus >> OTP_ECC_DBL_ERR_SHIFT) & OTP_ECC_DBL_ERR_MASK;
			ecccorrstatus = (eccstatus >> OTP_ECC_CORR_ST_SHIFT) & OTP_ECC_CORR_ST_MASK;
			eccenable = (eccstatus >> OTP_ECC_ENAB_SHIFT) & OTP_ECC_ENAB_MASK;

			printf(" : ecc st: %s, %s, derr %1d, ded %1d, sec %1d, synd 0x%2x",
				(OTP_ECC_ENAB(eccenable) ? "ecc ena" : "ecc dis"),
				((ecccorrstatus == OTP_ECC_MODE) ? "ecc md rd" : "no-ecc rd"),
				eccdblerr, eccdedst, eccsecst, eccdt);
		}

		numrows -= read_cmd.numrows;
		rowoffset += read_cmd.numrows;
	}

	printf("\n\n");

	return (0);
}

/* Variation: Like getandprint, but allow an int arg to be passed */
static int
wl_var_setintandprintstr(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int32 val;
	char *varname;
	char *endptr = NULL;

	UNUSED_PARAMETER(cmd);

	if (!*argv) {
		printf("set: missing arguments\n");
		return BCME_USAGE_ERROR;
	}

	varname = *argv++;

	if (!*argv) {
		val = 0;
	} else {
		val = strtol(*argv, &endptr, 0);
		if (*endptr != '\0') {
			/* not all the value string was parsed by strtol */
			printf("set: error parsing value \"%s\" as an integer for set of \"%s\"\n",
			       *argv, varname);
			return BCME_USAGE_ERROR;
		}
	}

	val = htod32(val);
	err = wlu_iovar_getbuf(wl, varname, &val, sizeof(int), buf, WLC_IOCTL_MAXLEN);

	if (err)
		return (err);

	printf("%s\n", buf);
	return (0);
}
