/*
 * wl bmac command module
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
 * $Id: wluc_bmac.c 774769 2019-05-07 08:46:22Z $
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
#include <bcmsrom_fmt.h>
#include <bcmsrom_tbl.h>

/* For backwards compatibility, the absense of the define 'NO_FILESYSTEM_SUPPORT'
 * implies that a filesystem is supported.
 */
#if !defined(BWL_NO_FILESYSTEM_SUPPORT)
#define BWL_FILESYSTEM_SUPPORT
#endif // endif

static cmd_func_t wl_gpioout;
static cmd_func_t wl_nvsource;
static cmd_func_t wl_var_getinthex;
static cmd_func_t wl_devpath;
static cmd_func_t wl_diag;
#if defined(WLTEST)
static cmd_func_t wlu_ccreg;
static cmd_func_t wlu_pmuchipctrlreg;
static cmd_func_t wlu_gcichipctrlreg;
#endif // endif
static cmd_func_t wlu_srwrite_data;

static cmd_t wl_bmac_cmds[] = {
	{ "srcrc", wlu_srwrite, WLC_GET_SROM, -1,
	"Get the CRC for input binary file" },
	{ "cis_source", wl_varint, WLC_GET_VAR, -1,
	"Display which source is used for the SDIO CIS"},
	{ "nvram_source", wl_nvsource, WLC_GET_VAR, -1,
	"Display which source is used for nvram"},
	{ "customvar1", wl_var_getinthex, -1, -1,
	"print the value of customvar1 in hex format" },
	{ "gpioout", wl_gpioout, -1, -1,
	"Set any GPIO pins to any value. Use with caution as GPIOs would be "
	"assigned to chipcommon\n"
	"\tUsage: gpiomask gpioval"},
	{ "devpath", wl_devpath, WLC_GET_VAR, -1,
	"print device path" },
#if defined(WLTEST)
	{ "ccreg", wlu_ccreg, WLC_GET_VAR, WLC_SET_VAR, "g/set cc registers"},
	{ "pcicfgreg", wlu_reg2args, WLC_GET_VAR, WLC_SET_VAR, "g/set pci cfg register"},
	{ "pmuccreg", wlu_pmuchipctrlreg, WLC_GET_VAR, WLC_SET_VAR,
	"g/set pmu chipcontrol registers"},
	{ "gciccreg", wlu_gcichipctrlreg, WLC_GET_VAR, WLC_SET_VAR,
	"g/set gci chipcontrol registers"},
#endif // endif
	{ "diag", wl_diag, WLC_GET_VAR, -1,
	"diag testindex(1-interrupt, 2-loopback, 3-memory, 4-led 9-loopback_ucode)\n"
	"    for index 1-5: precede by 'wl down' and follow by 'wl up'\n"
	"    for index 9:   precede by 'wl up', configure ibss"},
	{ "srwrite_data", wlu_srwrite_data, WLC_GET_SROM, WLC_SET_SROM,
	"Write caldata to srom: srwrite_data -t type filename\n"
	"\t Supported types: calblob"},
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_bmac_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register bmac commands */
	wl_module_cmds_register(wl_bmac_cmds);
}

static int
wl_gpioout(void *wl, cmd_t *cmd, char **argv)
{
	uint32 mask;
	uint32 val;
	char *endptr = NULL;
	uint argc;
	uint32 *int_ptr;

	UNUSED_PARAMETER(cmd);

	val = 0;

	/* eat command name */
	argv++;

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	/* Get and print the values */
	if (argc == 0) {
		uint32 gpio_cntrl;
		uint32 gpio_out;
		uint32 gpio_outen;
		int ret;

		if ((ret = wlu_iovar_get(wl, "gpioout", buf, sizeof(uint32) *3)) < 0)
			return ret;
		gpio_cntrl = dtoh32(((uint32 *)buf)[0]);
		gpio_out = dtoh32(((uint32 *)buf)[1]);
		gpio_outen = dtoh32(((uint32 *)buf)[2]);

		printf("gpiocontrol 0x%x gpioout 0x%x gpioouten 0x%x\n", gpio_cntrl,
		       gpio_out, gpio_outen);

		return 0;
	}

	/* required arg: mask value */
	if (argc < 2)
		return BCME_USAGE_ERROR;

	mask = strtoul(argv[0], &endptr, 0);
	if (*endptr != '\0')
		return BCME_USAGE_ERROR;

	val = strtoul(argv[1], &endptr, 0);
	if (*endptr != '\0')
		return BCME_USAGE_ERROR;

	if ((~mask & val) != 0)
		return BCME_BADARG;

	int_ptr = (uint32 *)buf;
	mask = htod32(mask);
	memcpy(int_ptr, (const void *)&mask, sizeof(mask));
	int_ptr++;
	val = htod32(val);
	memcpy(int_ptr, (const void *)&val, sizeof(val));

	return wlu_iovar_set(wl, "gpioout", buf, sizeof(uint32) *2);
}

static int
wl_nvsource(void *wl, cmd_t *cmd, char **argv)
{
	int32 val, err;

	if ((err = wl_var_get(wl, cmd, argv)))
		return (err);

	val = dtoh32(*(int32*)buf);

	switch (val) {
	case 0:
		printf("SROM\n");
		break;
	case 1:
		printf("OTP\n");
		break;
	case 2:
#ifdef WL_UNFNVRAM
		printf("UNFNVRAM\n");
#else
		printf("NVRAM\n");
#endif // endif
		break;
	default:
		printf("Unrecognized source %d\n", val);
		break;
	}

	return 0;
}

int wlu_srwrite_data(void *wl, cmd_t *cmd, char **argv)
{
	int ret, len, nw;
	uint16 *words = (uint16 *)&buf[8];
	uint16 caldata_offset;
	FILE *fp = NULL;
	srom_rw_t   *srt =  (srom_rw_t *)buf;
	char *arg;
	char *cal_buf;
	int argc;
	int sromrev = 0;

	for (argc = 0; argv[argc]; argc++);

	if (argc != 4)
		return BCME_USAGE_ERROR;

	/* We need at least one arg */
	if (!*++argv)
		return BCME_USAGE_ERROR;

	arg = *argv++;
	if (!strcmp(arg, "-t") && !strcmp(*argv++, "calblob")) {
		arg = *argv++;
	/*
	 * Avoid wl utility to driver compatibility issues by reading a 'safe' amount of words from
	 * SPROM to determine the SPROM version that the driver supports, once the version is known
	 * the full SPROM contents can be read. At the moment sromrev12 is the largest.
	 */
		nw = MAX(MAX(SROM10_SIGN, SROM11_SIGN), SROM11_SIGN)  + 1;
		srt->byteoff = htod32(0);
		srt->nbytes = htod32(2 * nw);

		if (cmd->get < 0)
			return BCME_ERROR;

		if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
			return ret;
		caldata_offset = words[SROM15_CAL_OFFSET_LOC] & 0xff;
		if (words[SROM11_SIGN] == SROM15_SIGNATURE) {
			nw = SROM15_WORDS;
		} else if (words[SROM16_SIGN] == SROM16_SIGNATURE) {
			nw = SROM16_WORDS;
			caldata_offset = words[SROM16_CALDATA_OFFSET_LOC];
			sromrev = 16;
			printf("Srom%d, byte offset: %d\n", sromrev, caldata_offset);
			if (caldata_offset == 0) {
				printf("caldata offset is not valid\n");
				return BCME_ERROR;
			}
		} else {
			printf("Unsupported for SROM revs other than rev15/16\n");
			return BCME_ERROR;
		}

		/* Reading caldata from msf file */
		if (!(fp = fopen(arg, "rb"))) {
				fprintf(stderr, "%s: No such file or directory\n", arg);
				return BCME_BADARG;
		}

		cal_buf = malloc(SROM_MAX);
		if (cal_buf == NULL) {
			ret = BCME_NOMEM;
			goto out;
		}
		len = fread(cal_buf, 1, SROM_MAX + 1, fp);
		len = (len + 1) & ~1;
		if (len > SROM15_MAX_CAL_SIZE) {
			ret = BCME_BUFTOOLONG;
			goto out;
		}
		if ((ret = ferror(fp))) {
			printf("\nerror %d reading %s\n", ret, arg);
			ret = BCME_ERROR;
			goto out;
		}
		if (!feof(fp)) {
			printf("\nFile %s is too large\n", arg);
			ret = BCME_ERROR;
			goto out;
		}
		if ((len - MAX_IOCTL_TXCHUNK_SIZE) > 0) {
			memcpy(srt->buf, cal_buf, MAX_IOCTL_TXCHUNK_SIZE);
			srt->byteoff = htod32(caldata_offset);
			srt->nbytes = htod32(MAX_IOCTL_TXCHUNK_SIZE);
			ret = wlu_set(wl, cmd->set, buf, MAX_IOCTL_TXCHUNK_SIZE + 8);
			memcpy(srt->buf, cal_buf + MAX_IOCTL_TXCHUNK_SIZE,
				len - MAX_IOCTL_TXCHUNK_SIZE);
			srt->byteoff = htod32(caldata_offset + MAX_IOCTL_TXCHUNK_SIZE);
			srt->nbytes = htod32(len - MAX_IOCTL_TXCHUNK_SIZE);
			ret = wlu_set(wl, cmd->set, buf, len - MAX_IOCTL_TXCHUNK_SIZE + 8);
		}
		else {
			memcpy(srt->buf, cal_buf, len);
			srt->byteoff = htod32(caldata_offset);
			srt->nbytes = htod32(len);
			ret = wlu_set(wl, cmd->set, buf, len + 8);
		}
	}
	else {
		printf("Invalid arguments for srwrite_data\n");
		return BCME_BADARG;
	}
out:
	fflush(stdout);
	if (fp)
		fclose(fp);
	free(cal_buf);
	return ret;
}

#if defined(WLTEST)
static int
wlu_ccreg(void *wl, cmd_t *cmd, char **argv)
{
	char var[256];
	uint32 int_val;
	bool get = TRUE;
	uint32 len;
	void *ptr = NULL;
	char *endptr;
	int ret = 0;

	if (argv[1]) {
		len = sizeof(int_val);
		int_val = htod32(strtoul(argv[1], &endptr, 0));
		memcpy(var, (char *)&int_val, sizeof(int_val));
	}
	else
		return BCME_USAGE_ERROR;

	if (argv[2]) {
		get = FALSE;
		int_val = htod32(strtoul(argv[2], &endptr, 0));
		memcpy(&var[len], (char *)&int_val, sizeof(int_val));
		len += sizeof(int_val);
	}
	if (get) {
		if ((ret = (wlu_var_getbuf(wl, cmd->name, var, sizeof(var), &ptr))) < 0)
			return ret;

		printf("0x%x\n", dtoh32(*(int *)ptr));
	}
	else
		ret = wlu_var_setbuf(wl, cmd->name, &var, sizeof(var));
	return ret;
}

static int
wlu_pmuchipctrlreg(void *wl, cmd_t *cmd, char **argv)
{
	char var[256];
	uint32 int_val;
	bool get = TRUE;
	uint32 len;
	void *ptr = NULL;
	char *endptr;
	int ret = 0;

	if (argv[1]) {
		len = sizeof(int_val);
		int_val = htod32(strtoul(argv[1], &endptr, 0));
		memcpy(var, (char *)&int_val, sizeof(int_val));
	} else {
		return BCME_USAGE_ERROR;
	}

	if (argv[2]) {
		get = FALSE;
		int_val = htod32(strtoul(argv[2], &endptr, 0));
		memcpy(&var[len], (char *)&int_val, sizeof(int_val));
		len += sizeof(int_val);
	}
	if (get) {
		if ((ret = (wlu_var_getbuf(wl, cmd->name, var, sizeof(var), &ptr))) < 0) {
			return ret;
		}

		printf("0x%x\n", dtoh32(*(int *)ptr));
	} else {
		ret = wlu_var_setbuf(wl, cmd->name, &var, sizeof(var));
	}
	return ret;
}

static int
wlu_gcichipctrlreg(void *wl, cmd_t *cmd, char **argv)
{
	char var[256];
	uint32 int_val;
	bool get = TRUE;
	uint32 len;
	void *ptr = NULL;
	char *endptr;
	int ret = 0;

	if (argv[1]) {
		len = sizeof(int_val);
		int_val = htod32(strtoul(argv[1], &endptr, 0));
		memcpy(var, (char *)&int_val, sizeof(int_val));
	} else {
		return BCME_USAGE_ERROR;
	}

	if (argv[2]) {
		get = FALSE;
		int_val = htod32(strtoul(argv[2], &endptr, 0));
		memcpy(&var[len], (char *)&int_val, sizeof(int_val));
		len += sizeof(int_val);
	}
	if (get) {
		if ((ret = (wlu_var_getbuf(wl, cmd->name, var, sizeof(var), &ptr))) < 0) {
			return ret;
		}

		printf("0x%x\n", dtoh32(*(int *)ptr));
	} else {
		ret = wlu_var_setbuf(wl, cmd->name, &var, sizeof(var));
	}
	return ret;
}
#endif // endif

/*
 * wlu_reg3args is a generic function that is used for setting/getting
 * WL_IOVAR variables that require address + offset for read, and
 * address + offset + data for write.
 */
int
wlu_reg3args(void *wl, cmd_t *cmd, char **argv)
{
	char var[256];
	uint32 int_val;
	bool get = TRUE;
	uint32 len, i;
	void *ptr = NULL;
	char *endptr;
	uint numargs;
	int ret = 0;

	len = 0;

	if (!argv[1] || !argv[2]) {
		printf("Wrong syntax => dev offset [val]\n");
		return BCME_USAGE_ERROR;
	}

	if (argv[3]) {
		numargs = 3;
		get = FALSE;
	} else
		numargs = 2;

	for (i = 1; i <= numargs; i++) {
		int_val = htod32(strtoul(argv[i], &endptr, 0));
		memcpy(&var[len], (char *)&int_val, sizeof(int_val));
		len += sizeof(int_val);
	}

	if (get) {
		if ((ret = wlu_var_getbuf(wl, cmd->name, var, sizeof(var), &ptr)) < 0)
			return ret;

		printf("0x%x\n", dtoh32(*(int *)ptr));
	}
	else
		ret = wlu_var_setbuf(wl, cmd->name, &var, sizeof(var));
	return ret;
}

static int
wl_var_getinthex(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int32 val;

	if ((err = wl_var_get(wl, cmd, argv)))
		return (err);

	val = dtoh32(*(int32*)buf);

	printf("0x%08x\n", val);

	return 0;
}

void
wl_printlasterror(void *wl)
{
	char error_str[128];

	if (wlu_iovar_get(wl, "bcmerrorstr", error_str, sizeof(error_str)) != 0) {
		fprintf(stderr, "%s: \nError getting the last error\n", wlu_av0);
	} else {
		fprintf(stderr, "%s: %s\n", wlu_av0, error_str);
	}
}

static int
wl_devpath(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	void *ptr;
	char *pbuf = buf;

	UNUSED_PARAMETER(argv);

	if ((err = wlu_var_getbuf_sm (wl, cmd->name, NULL, 0, &ptr)))
		return (err);

	pbuf += strlen(buf);
	sprintf(pbuf, "\n");
	fputs(buf, stdout);
	return (0);
}

static int
wl_diag(void *wl, cmd_t *cmd, char **argv)
{
	uint testindex;
	int buflen, err;
	char *param;
	uint32 testresult;

	if (!*++argv) {
		printf(" Usage: %s testindex[1-4]\n", cmd->name);
		return BCME_USAGE_ERROR;
	}

	testindex = atoi(*argv);

	strcpy(buf, "diag");
	buflen = strlen(buf) + 1;
	param = (char *)(buf + buflen);
	testindex = htod32(testindex);
	memcpy(param, (char*)&testindex, sizeof(testindex));

	if ((err = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
		return err;

	testresult = *(uint32 *)buf;
	testindex = dtoh32(testindex);
	testresult = dtoh32(testresult);
	if (testresult != 0) {
		printf("\ndiag test %d failed(error code %d)\n", testindex, testresult);
	} else
		printf("\ndiag test %d passed\n", testindex);

	return (0);
}
