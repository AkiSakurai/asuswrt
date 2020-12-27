/*
 * wl msch command module
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
 * $Id: wluc_msch.c 774769 2019-05-07 08:46:22Z $
 */

#include <wlioctl.h>

#if defined(DONGLEBUILD)
#include <typedefs.h>
#include <osl.h>
#endif // endif

#include <bcmutils.h>
#include <bcmendian.h>
#include "wlu_common.h"
#include "wlu.h"

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/if_packet.h>

#include <miniopt.h>
#include <sys/stat.h>
#include <trxhdr.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <event_log.h>

#if defined(EDK_RELEASE_VERSION) || (EDK_RELEASE_VERSION >= 0x00020000)
extern int wlu_efi_stat(char *filename, struct stat *filest);
extern long wlu_efi_ftell(void *fp);
extern int wlu_efi_fseek(void *fp, long offset, int whence);
extern size_t wlu_efi_fwrite(void *buf, size_t size, size_t nmemb, void *fp);
extern size_t wlu_efi_fread(void *buf, size_t size, size_t nmemb, void *fp);
extern void wlu_efi_fclose(void *fp);
extern void * wlu_efi_fopen(char *filename, char *mode);

#define fopen(filename, mode)		(FILE *)wlu_efi_fopen(filename, mode)
#define fread(buf, size, nmemb, fp)	wlu_efi_fread(buf, size, nmemb, fp)
#define fwrite(buf, size, nmemb, fp)	wlu_efi_fwrite(buf, size, nmemb, fp)
#define fseek(fp, offset, origin)	wlu_efi_fseek(fp, offset, origin)
#define ftell(fp)			wlu_efi_ftell(fp)
#define stat(fname, filest)		wlu_efi_stat(fname, (struct stat *)(filest))
#define fclose(fp)			wlu_efi_fclose(fp)
#ifdef stderr
#undef stderr
#define stderr stdout
#endif // endif
#endif /* defined(EDK_RELEASE_VERSION) || (EDK_RELEASE_VERSION >= 0x00020000) */

#if defined(linux)
static cmd_func_t wl_msch_event_check;
#endif   /* linux */
static cmd_func_t wl_msch_request;
static cmd_func_t wl_msch_collect;
static cmd_func_t wl_msch_dump;
static cmd_func_t wl_msch_profiler;

static char *mschbufp = NULL, *mschdata = NULL;
static FILE *mschfp = NULL;

#define MSCH_COLLECT_USAGE \
	"{enable|disable} [+/-register] [+/-callback] [+/-profiler] " \
	"[+/-error] [+/-debug] [+/-info] [+/-trace]\n" \
	"\tenable: alloc memory to save profiler data\n" \
	"\tdisable: stop monitor, and free memory\n" \
	"\t+/-register: save or skip register data\n" \
	"\t+/-callback: save or skip register data\n" \
	"\t+/-profiler: save or skip profiler data\n" \
	"\t+/-error: save or skip error messages\n" \
	"\t+/-debug: save or skip debug messages\n" \
	"\t+/-info: save or skip infomation messages\n" \
	"\t+/-trace: save or skip trace messages\n"

#define MSCH_FMTFILE_USAGE \
	"-D copydir -F \"logstrs_path=xxxx st_str_file_path=xxxx " \
	"map_file_path=xxxx rom_st_str_file_path=xxxx rom_map_file_path=xxxx\"\n"

static cmd_t wl_msch_cmds[] = {
	{ "msch_req", wl_msch_request, WLC_GET_VAR, WLC_SET_VAR,
	"register multiple channel scheduler \n"
	"\tUsage: msch_req <id> -w wlc_index -c \"channel_list\" -f flags -t type [params] "
	"-p priority -s start_time -d duration -i interval"
	"\t-w W, --wlc_index=W\tset wlc index\n"
	"\t-c L, --channels=L\tcomma or space separated list of channels to scheduler\n"
	"\t-f F, --flags=F\tset flags, 1 for contiguous cahhhel scheduler\n"
	"\t-t T, --type=T\tset scheduler type: fix, sf, df dur-flex, bf params\n\n"
	"\t-p P, --priority=P\tpriority for the scheduler\n"
	"\t-s S, --start-time=S\tstart time(ms) for the scheduler\n"
	"\t-d D, --duration=D\tduration(ms) for the scheduler\n"
	"\t-i I, --interval=I\tinterval(ms) for the scheduler\n"},

	{ "msch_collect", wl_msch_collect, WLC_GET_VAR, WLC_SET_VAR,
	"control multiple channel scheduler profiler saving data\n"
	"\tUsage: msch_collect " MSCH_COLLECT_USAGE},

	{ "msch_dump", wl_msch_dump, WLC_GET_VAR, WLC_SET_VAR,
	"dump multiple channel scheduler profiler data\n"
	"\tUsage: msch_dump [filename] " MSCH_FMTFILE_USAGE},

	{ "msch_profiler", wl_msch_profiler, WLC_GET_VAR, WLC_SET_VAR,
	"dump multiple channel scheduler profiler data\n"
	"\tUsage: msch_profiler [uploadfilename] [filename] " MSCH_FMTFILE_USAGE},

	{ "msch_event", wl_msch_collect, WLC_GET_VAR, WLC_SET_VAR,
	"control multiple channel scheduler profiler event data\n"
	"\tUsage: msch_event " MSCH_COLLECT_USAGE},

	{ "msch_event_log", wl_msch_collect, WLC_GET_VAR, WLC_SET_VAR,
	"control multiple channel scheduler profiler event Log data\n"
	"\tUsage: msch_event_log " MSCH_COLLECT_USAGE},

#if defined(linux)
	{ "msch_event_check", wl_msch_event_check, -1, -1,
	"Listen and print Multiple channel scheduler events\n"
	"\tmsch_event_check syntax is: msch_event_check ifname [filename]"
	"[+/-register] [+/-callback] [+/-profiler] "
	"[+/-error] [+/-debug] [+/-info] [+/-trace] " MSCH_FMTFILE_USAGE},
#endif   /* linux */
	{ NULL, NULL, 0, 0, NULL }
};

/* module initialization */
void
wluc_msch_module_init(void)
{
	/* get the global buf */
	if (WLC_IOCTL_MAXLEN >= 2 * WL_MSCH_PROFILER_BUFFER_SIZE) {
		mschdata = wl_get_buf();
	} else {
		mschdata = (char *)malloc(2 * WL_MSCH_PROFILER_BUFFER_SIZE);
		if (!mschdata)
			return;
	}
	mschbufp = (char *)&mschdata[WL_MSCH_PROFILER_BUFFER_SIZE];

	/* register proxd commands */
	wl_module_cmds_register(wl_msch_cmds);
}

static int
wl_parse_msch_chanspec_list(char *list_str, chanspec_t *chanspec_list, int chanspec_num)
{
	int num = 0;
	chanspec_t chanspec;
	char *next, str[8];
	size_t len;

	if ((next = list_str) == NULL)
		return BCME_ERROR;

	while ((len = strcspn(next, " ,")) > 0) {
		if (len >= sizeof(str)) {
			fprintf(stderr, "string \"%s\" before ',' or ' ' is too long\n", next);
			return BCME_ERROR;
		}
		strncpy(str, next, len);
		str[len] = 0;
		chanspec = wf_chspec_aton(str);
		if (chanspec == 0) {
			fprintf(stderr, "could not parse chanspec starting at "
			        "\"%s\" in list:\n%s\n", str, list_str);
			return BCME_ERROR;
		}
		if (num == chanspec_num) {
			fprintf(stderr, "too many chanspecs (more than %d) in chanspec list:\n%s\n",
				chanspec_num, list_str);
			return BCME_ERROR;
		}
		chanspec_list[num++] = htodchanspec(chanspec);
		next += len;
		next += strspn(next, " ,");
	}

	return num;
}

static int
wl_parse_msch_bf_param(char *param_str, uint32 *param)
{
	int num;
	uint32 val;
	char* str;
	char* endptr = NULL;

	if (param_str == NULL)
		return BCME_ERROR;

	str = param_str;
	num = 0;
	while (*str != '\0') {
		val = (uint32)strtol(str, &endptr, 0);
		if (endptr == str) {
			fprintf(stderr,
				"could not parse bf param starting at"
				" substring \"%s\" in list:\n%s\n",
				str, param_str);
			return -1;
		}
		str = endptr + strspn(endptr, " ,");

		if (num >= 4) {
			fprintf(stderr, "too many bf param (more than 6) in param str:\n%s\n",
				param_str);
			return BCME_ERROR;
		}

		param[num++] = htod32(val);
	}

	return num;
}

static int
wl_msch_request(void *wl, cmd_t *cmd, char **argv)
{
	int params_size = sizeof(msch_register_params_t);
	msch_register_params_t *params;
	uint32 val = 0;
	uint16 val16;
	char key[64];
	int keylen;
	char *p, *eq, *valstr, *endptr = NULL;
	char opt;
	bool good_int;
	int err = BCME_OK;
	int i;

	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(cmd);

	params = (msch_register_params_t*)malloc(params_size);
	if (params == NULL) {
		fprintf(stderr, "Error allocating %d bytes for msch register params\n",
			params_size);
		return BCME_NOMEM;
	}
	memset(params, 0, params_size);

	/* skip the command name */
	argv++;

	params->id = 0;

	while ((p = *argv) != NULL) {
		argv++;
		memset(key, 0, sizeof(key));
		opt = '\0';
		valstr = NULL;
		good_int = FALSE;

		if (!strncmp(p, "--", 2)) {
			eq = strchr(p, '=');
			if (eq == NULL) {
				fprintf(stderr,
				"wl_msch_request: missing \" = \" in long param \"%s\"\n", p);
				err = BCME_USAGE_ERROR;
				goto exit;
			}
			keylen = eq - (p + 2);
			if (keylen > 63)
				keylen = 63;
			memcpy(key, p + 2, keylen);

			valstr = eq + 1;
			if (*valstr == '\0') {
				fprintf(stderr, "wl_msch_request: missing value after "
					"\" = \" in long param \"%s\"\n", p);
				err = BCME_USAGE_ERROR;
				goto exit;
			}
		}
		else if (!strncmp(p, "-", 1)) {
			opt = p[1];
			if (strlen(p) > 2) {
				fprintf(stderr, "wl_msch_request: only single char options, "
					"error on param \"%s\"\n", p);
				err = BCME_BADARG;
				goto exit;
			}
			if (*argv == NULL) {
				fprintf(stderr,
				"wl_msch_request: missing value parameter after \"%s\"\n", p);
				err = BCME_USAGE_ERROR;
				goto exit;
			}
			valstr = *argv;
			argv++;
		} else {
			val = (uint32)strtol(p, &endptr, 0);
			if (*endptr == '\0') {
				/* not all the value string was parsed by strtol */
				good_int = TRUE;
			}
			if (!good_int) {
				fprintf(stderr,
				"wl_msch_request: parameter error \"%s\"\n", p);
				err = BCME_BADARG;
				goto exit;
			}
			val16 = (uint16)val;
			params->id = htod16(val16);
			continue;
		}

		/* parse valstr as int just in case */
		val = (uint32)strtol(valstr, &endptr, 0);
		if (*endptr == '\0') {
			/* not all the value string was parsed by strtol */
			good_int = TRUE;
		}

		if (opt == 'h' || !strcmp(key, "help")) {
			printf("%s", cmd->help);
			goto exit;
		} else if (opt == 'w' || !strcmp(key, "wlc_index")) {
			if (!good_int) {
				fprintf(stderr,
				"could not parse \"%s\" as an wlc_index\n",
					valstr);
				err = BCME_BADARG;
				goto exit;
			}
			val16 = (uint16)val;
			params->wlc_index = htod16(val16);
		} else if (opt == 'c' || !strcmp(key, "channels")) {
			i = wl_parse_msch_chanspec_list(valstr,
				(chanspec_t *)params->chanspec_list, WL_MSCH_NUMCHANNELS);
			if (i == BCME_ERROR) {
				fprintf(stderr, "error parsing channel list arg\n");
				err = BCME_BADARG;
				goto exit;
			}
			val = (uint32)i;
			params->chanspec_cnt = htod32(val);
		} else if (opt == 'n' || !strcmp(key, "id")) {
			if (!good_int) {
				fprintf(stderr,
				"could not parse \"%s\" as an register id\n",
					valstr);
				err = BCME_BADARG;
				goto exit;
			}
			val16 = (uint16)val;
			params->id = htod16(val16);
		} else if (opt == 'f' || !strcmp(key, "flags")) {
			if (!good_int) {
				fprintf(stderr,
				"could not parse \"%s\" as an flages\n",
					valstr);
				err = BCME_BADARG;
				goto exit;
			}
			val16 = (uint16)val;
			params->flags = htod16(val16);
		} else if (opt == 't' || !strcmp(key, "type")) {
			if (!strcmp(valstr, "f") || !strncmp(valstr, "fix", 3)) {
				params->req_type = (uint32)WL_MSCH_RT_BOTH_FIXED;
			} else if (!strcmp(valstr, "sf") || !strcmp(valstr, "start-flex")) {
				params->req_type = (uint32)WL_MSCH_RT_START_FLEX;
			} else if (!strcmp(valstr, "df") || !strcmp(valstr, "dur-flex")) {
				if (*argv == NULL) {
					fprintf(stderr,
					"wl_msch_request: missing param of dur-flex\n");
					err = BCME_USAGE_ERROR;
					goto exit;
				}
				valstr = *argv;
				argv++;

				val = (uint32)strtol(valstr, &endptr, 0);
				if (*endptr != '\0') {
					fprintf(stderr,
					"could not parse \"%s\" as dur-flex value\n", valstr);
					err = BCME_USAGE_ERROR;
					goto exit;
				}

				params->req_type = (uint32)WL_MSCH_RT_DUR_FLEX;
				params->dur_flex = htod32(val);
			} else if (!strcmp(valstr, "bf") || !strcmp(valstr, "both-flex")) {
				if (*argv == NULL) {
					fprintf(stderr,
					"wl_msch_request: missing param of both-flex\n");
					err = BCME_USAGE_ERROR;
					goto exit;
				}
				valstr = *argv;
				argv++;

				if (wl_parse_msch_bf_param(valstr, &params->min_dur) != 4) {
					fprintf(stderr, "error parsing both flex params\n");
					err = BCME_BADARG;
					goto exit;
				}
				params->req_type = (uint32)WL_MSCH_RT_BOTH_FLEX;
			} else {
				fprintf(stderr,
				"error type param \"%s\"\n",
					valstr);
				err = BCME_BADARG;
				goto exit;
			}

			params->req_type = htod32(params->req_type);
		} else if (opt == 'p' || !strcmp(key, "priority")) {
			if (!good_int) {
				fprintf(stderr,
				"could not parse \"%s\" as priority\n",
					valstr);
				err = BCME_BADARG;
				goto exit;
			}
			val16 = (uint16)val;
			params->priority = htod16(val16);
		} else if (opt == 's' || !strcmp(key, "start-time")) {
			if (!good_int) {
				fprintf(stderr,
				"could not parse \"%s\" as start time\n",
					valstr);
				err = BCME_BADARG;
				goto exit;
			}
			params->start_time = htod32(val);
		} else if (opt == 'd' || !strcmp(key, "duration")) {
			if (!good_int) {
				fprintf(stderr,
				"could not parse \"%s\" as duration\n",
					valstr);
				err = BCME_BADARG;
				goto exit;
			}
			params->duration = htod32(val);
		} else if (opt == 'i' || !strcmp(key, "interval")) {
			if (!good_int) {
				fprintf(stderr,
				"could not parse \"%s\" as interval\n",
					valstr);
				err = BCME_BADARG;
				goto exit;
			}
			params->interval = htod32(val);
		} else {
			fprintf(stderr,
			"wl_msch_request: error option param \"%s\"\n", p);
			err = BCME_BADARG;
			goto exit;
		}
	}

	err = wlu_var_setbuf(wl, cmd->name, params, params_size);
exit:
	free(params);
	return err;
}

static int
wl_msch_collect(void *wl, cmd_t *cmd, char **argv)
{
	bool eventcmd = (!strcmp(cmd->name, "msch_event"));
	bool eventlogcmd = (!strcmp(cmd->name, "msch_event_log"));
	bool collectcmd = (!strcmp(cmd->name, "msch_collect"));
	char *p, *endptr;
	int opt, v, val, err = BCME_OK;

	UNUSED_PARAMETER(wl);

	if ((err = wlu_iovar_getint(wl, cmd->name, &val)) < 0)
		return err;

	err = BCME_OK;
	if (!*++argv) {
		printf("MSCH %s: %sable {", (eventcmd? "Event" : (eventlogcmd? "EventLog" :
			"Profile")), ((val & WL_MSCH_CMD_ENABLE_BIT)? "En" : "Dis"));
		if (val & WL_MSCH_CMD_REGISTER_BIT)
			printf("registe ");
		if (val & WL_MSCH_CMD_CALLBACK_BIT)
			printf("callbac ");
		if (val & WL_MSCH_CMD_PROFILE_BIT)
			printf("profiler ");
		if (val & WL_MSCH_CMD_ERROR_BIT)
			printf("error ");
		if (val & WL_MSCH_CMD_DEBUG_BIT)
			printf("debug ");
		if (val & WL_MSCH_CMD_INFOM_BIT)
			printf("info ");
		if (val & WL_MSCH_CMD_TRACE_BIT)
			printf("trace ");
		printf("\x08}\n");
		return err;
	}

	while ((p = *argv) != NULL) {
		opt = 0;
		if (p[0] == '+') {
			opt = 1;
			p++;
		}
		else if (p[0] == '-') {
			opt = 2;
			p++;
		}

		if (opt == 0) {
			v = (uint32)strtol(p, &endptr, 0);
			if (*endptr == '\0') {
				if (v == 1)
					val |= WL_MSCH_CMD_ENABLE_BIT;
				else if (v == 0)
					val &= ~WL_MSCH_CMD_ENABLE_BIT;
				else {
					err = BCME_EPERM;
					break;
				}
			}
			else if (!strcmp(p, "enable") || !strcmp(p, "start")) {
				val |= WL_MSCH_CMD_ENABLE_BIT;
			}
			else if (!strcmp(p, "disable") || !strcmp(p, "stop"))
				val &= ~WL_MSCH_CMD_ENABLE_BIT;
			else if (collectcmd && !strcmp(p, "dump"))
				return wl_msch_dump(wl, cmd, argv);
			else {
				err = BCME_EPERM;
				break;
			}
		} else {
			if (opt == 2 && (!strcmp(p, "-help") || !strcmp(p, "h"))) {
				printf("%s", cmd->help);
				return BCME_OK;
			} else if (opt == 2 && (!strcmp(p, "all") || !strcmp(p, "a"))) {
				val &= ~WL_MSCH_CMD_ALL_BITS;
			} else if (!strcmp(p, "profiler") || !strcmp(p, "p")) {
				if (opt == 1)
					val |= WL_MSCH_CMD_PROFILE_BIT;
				else
					val &= ~WL_MSCH_CMD_PROFILE_BIT;
			}
			else if (!strcmp(p, "callback") || !strcmp(p, "c")) {
				if (opt == 1)
					val |= WL_MSCH_CMD_CALLBACK_BIT;
				else
					val &= ~WL_MSCH_CMD_CALLBACK_BIT;
			}
			else if (!strcmp(p, "register") || !strcmp(p, "r")) {
				if (opt == 1)
					val |= WL_MSCH_CMD_REGISTER_BIT;
				else
					val &= ~WL_MSCH_CMD_REGISTER_BIT;
			}
			else if (!strcmp(p, "error") || !strcmp(p, "e")) {
				if (opt == 1)
					val |= WL_MSCH_CMD_ERROR_BIT;
				else
					val &= ~WL_MSCH_CMD_ERROR_BIT;
			}
			else if (!strcmp(p, "debug") || !strcmp(p, "d")) {
				if (opt == 1)
					val |= WL_MSCH_CMD_DEBUG_BIT;
				else
					val &= ~WL_MSCH_CMD_DEBUG_BIT;
			}
			else if (!strcmp(p, "info") || !strcmp(p, "i")) {
				if (opt == 1)
					val |= WL_MSCH_CMD_INFOM_BIT;
				else
					val &= ~WL_MSCH_CMD_INFOM_BIT;
			}
			else if (!strcmp(p, "trace") || !strcmp(p, "t")) {
				if (opt == 1)
					val |= WL_MSCH_CMD_TRACE_BIT;
				else
					val &= ~WL_MSCH_CMD_TRACE_BIT;
			}
			else if (collectcmd && opt == 2 && !strcmp(p, "s") && *++argv) {
				p = *argv;
				v = (uint32)strtol(p, &endptr, 0);
				if (*endptr == '\0') {
					val &= ~WL_MSCH_CMD_SIZE_MASK;
					val |= (v << WL_MSCH_CMD_SIZE_SHIFT);
				} else {
					err = BCME_EPERM;
					break;
				}
			}
			else {
				err = BCME_EPERM;
				break;
			}
		}

		argv++;
	}

	if (eventcmd && (val & WL_MSCH_CMD_ENABLE_BIT)) {
		uint8 event_inds_mask[WL_EVENTING_MASK_LEN];
		/*  read current mask state  */
		if ((err = wlu_iovar_get(wl, "event_msgs", &event_inds_mask,
			WL_EVENTING_MASK_LEN))) {
			fprintf(stderr, "couldn't read event_msgs\n");
			goto exit;
		}
		event_inds_mask[WLC_E_MSCH / 8] |= (1 << (WLC_E_MSCH % 8));
		if ((err = wlu_iovar_set(wl, "event_msgs", &event_inds_mask,
			WL_EVENTING_MASK_LEN))) {
			fprintf(stderr, "couldn't write event_msgs\n");
			goto exit;
		}
	}

	val &= ~WL_MSCH_CMD_VER_MASK;
	val |= (WL_MSCH_PROFILER_VER << WL_MSCH_CMD_VER_SHIFT);
	if (err == BCME_OK)
		err = wlu_iovar_setint(wl, cmd->name, val);
exit:
	return err;
}

typedef struct {
	int  num_fmts;
	char **fmts;
	char *raw_fmts;
	char *raw_sstr;
	uint32 fmts_size;
	uint32 raw_fmts_size;
	uint32 raw_sstr_size;
	uint32 ramstart;
	uint32 rodata_start;
	uint32 rodata_end;
	char *rom_raw_sstr;
	uint32 rom_raw_sstr_size;
	uint32 rom_ramstart;
	uint32 rom_rodata_start;
	uint32 rom_rodata_end;
} wl_event_log_t;

wl_event_log_t raw_event;

#define BYTES_AHEAD_NUM		11	/* address in map file is before these many bytes */
#define READ_NUM_BYTES		1000 /* read map file each time this No. of bytes */
#define GO_BACK_FILE_POS_NUM_BYTES	100 /* set file pos back to cur pos */
static char *ramstart_str = "text_start"; /* string in mapfile has addr ramstart */
static char *rodata_start_str = "rodata_start"; /* string in mapfile has addr rodata start */
static char *rodata_end_str = "rodata_end"; /* string in mapfile has addr rodata end */
#define RAMSTART_BIT	0x01
#define RDSTART_BIT		0x02
#define RDEND_BIT		0x04
#define ALL_MAP_VAL		(RAMSTART_BIT | RDSTART_BIT | RDEND_BIT)

static char *logstrs_path = "/root/logstrs.bin";
static char *st_str_file_path = "/root/rtecdc.bin";
static char *map_file_path = "/root/rtecdc.map";
static char *rom_st_str_file_path = "/root/roml.bin";
static char *rom_map_file_path = "/root/roml.map";
static char *ram_file_str = "rtecdc";
static char *rom_file_str = "roml";

static int
wl_read_map(char *fname, uint32 *ramstart, uint32 *rodata_start, uint32 *rodata_end)
{
	FILE *filep = NULL;
	char *raw_fmts =  NULL;
	int read_size = READ_NUM_BYTES;
	int keep_size = GO_BACK_FILE_POS_NUM_BYTES;
	int error = 0;
	char * cptr = NULL;
	char c;
	uint8 count = 0;

	if (fname == NULL) {
		fprintf(stderr, "%s: ERROR fname is NULL \n", __FUNCTION__);
		return BCME_ERROR;
	}

	filep = fopen(fname, "rb");
	if (!filep) {
		perror(fname);
		fprintf(stderr, "Cannot open file %s\n", fname);
		return BCME_ERROR;
	}

	*ramstart = 0;
	*rodata_start = 0;
	*rodata_end = 0;

	/* Allocate 1 byte more than read_size to terminate it with NULL */
	raw_fmts = (char *)malloc(read_size + keep_size + 1);
	if (raw_fmts == NULL) {
		fprintf(stderr, "%s: Failed to allocate raw_fmts memory \n", __FUNCTION__);
		goto fail;
	}
	memset(raw_fmts, ' ', GO_BACK_FILE_POS_NUM_BYTES);

	/* read ram start, rodata_start and rodata_end values from map	file */
	while (count != ALL_MAP_VAL)
	{
		error = fread(&raw_fmts[keep_size], 1, read_size, filep);
		if (error < 0) {
			fprintf(stderr, "%s: map file read failed err:%d \n", __FUNCTION__,
				error);
			goto fail;
		}

		/* End raw_fmts with NULL as strstr expects NULL terminated strings */
		raw_fmts[read_size + keep_size] = '\0';

		/* Get ramstart address */
		if ((cptr = strstr(raw_fmts, ramstart_str))) {
			cptr = cptr - BYTES_AHEAD_NUM;
			sscanf(cptr, "%x %c text_start", ramstart, &c);
			count |= RAMSTART_BIT;
		}

		/* Get ram rodata start address */
		if ((cptr = strstr(raw_fmts, rodata_start_str))) {
			cptr = cptr - BYTES_AHEAD_NUM;
			sscanf(cptr, "%x %c rodata_start", rodata_start, &c);
			count |= RDSTART_BIT;
		}

		/* Get ram rodata end address */
		if ((cptr = strstr(raw_fmts, rodata_end_str))) {
			cptr = cptr - BYTES_AHEAD_NUM;
			sscanf(cptr, "%x %c rodata_end", rodata_end, &c);
			count |= RDEND_BIT;
		}

		if (error < read_size) {
			/*
			 * since we reset file pos back to earlier pos by
			 * GO_BACK_FILE_POS_NUM_BYTES bytes we won't reach EOF.
			 * The reason for this is if string is spreaded across
			 * bytes, the read function should not miss it.
			 * So if ret value is less than read_size, reached EOF don't read further
			 */
			break;
		}
		/*
		* go back to predefined NUM of bytes so that we won't miss
		* the string and  addr even if it comes as splited in next read.
		*/
		memcpy(raw_fmts, &raw_fmts[read_size], keep_size);
	}

fail:
	if (filep) {
		fclose(filep);
	}

	if (raw_fmts) {
		free(raw_fmts);
	}

	if (count != ALL_MAP_VAL) {
		fprintf(stderr, "%s: readmap error 0X%x \n", __FUNCTION__, count);
		return BCME_ERROR;
	}
	return BCME_OK;
}

static void
wl_init_static_strs_array(char *str_file, char *map_file)
{
	FILE *filep = NULL;
	char *raw_fmts =  NULL;
	uint32 logstrs_size = 0;

	int error = 0;
	uint32 ramstart = 0;
	uint32 rodata_start = 0;
	uint32 rodata_end = 0;
	uint32 logfilebase = 0;

	error = wl_read_map(map_file, &ramstart, &rodata_start, &rodata_end);
	if (error != BCME_OK) {
		fprintf(stderr, "readmap Error!! \n");
		/* don't do event log parsing in actual case */
		if (strstr(str_file, ram_file_str) != NULL) {
			raw_event.raw_sstr = NULL;
		} else if (strstr(str_file, rom_file_str) != NULL) {
			raw_event.rom_raw_sstr = NULL;
		}
		return;
	}

	if (str_file == NULL) {
		fprintf(stderr, "%s: ERROR fname is NULL \n", __FUNCTION__);
		return;
	}

	filep = fopen(str_file, "rb");
	if (!filep) {
		perror(str_file);
		fprintf(stderr, "Cannot open file %s\n", str_file);
		return;
	}

	/* Full file size is huge. Just read required part */
	logstrs_size = rodata_end - rodata_start;

	raw_fmts = (char *)malloc(logstrs_size);
	if (raw_fmts == NULL) {
		fprintf(stderr, "%s: Failed to allocate raw_fmts memory \n", __FUNCTION__);
		goto fail;
	}

	logfilebase = rodata_start - ramstart;

	error = fseek(filep, logfilebase, SEEK_SET);
	if (error < 0) {
		fprintf(stderr, "%s: %s llseek failed %d \n", __FUNCTION__, str_file, error);
		goto fail;
	}

	error = fread(raw_fmts, 1, logstrs_size, filep);
	if (error != (int)logstrs_size) {
		fprintf(stderr, "%s: %s read failed %d \n", __FUNCTION__, str_file, error);
		goto fail;
	}

	if (strstr(str_file, ram_file_str) != NULL) {
		raw_event.raw_sstr = raw_fmts;
		raw_event.raw_sstr_size = logstrs_size;
		raw_event.ramstart = ramstart;
		raw_event.rodata_start = rodata_start;
		raw_event.rodata_end = rodata_end;
	} else if (strstr(str_file, rom_file_str) != NULL) {
		raw_event.rom_raw_sstr = raw_fmts;
		raw_event.rom_raw_sstr_size = logstrs_size;
		raw_event.rom_ramstart = ramstart;
		raw_event.rom_rodata_start = rodata_start;
		raw_event.rom_rodata_end = rodata_end;
	}

	fclose(filep);

	free(raw_fmts);

	return;

fail:
	if (raw_fmts) {
		free(raw_fmts);
	}

	if (filep) {
		fclose(filep);
	}

	if (strstr(str_file, ram_file_str) != NULL) {
		raw_event.raw_sstr = NULL;
	} else if (strstr(str_file, rom_file_str) != NULL) {
		raw_event.rom_raw_sstr = NULL;
	}

	return;
}

static void
wl_init_logstrs_array(char *logstrs_path)
{
	FILE *filep = NULL;
	char *raw_fmts =  NULL;
	int logstrs_size = 0;
	int error = 0;
	logstr_header_t *hdr = NULL;
	uint32 *lognums = NULL;
	char *logstrs = NULL;
	int ram_index = 0;
	char **fmts = NULL;
	int num_fmts = 0;
	int i = 0;

	if (logstrs_path == NULL) {
		fprintf(stderr, "%s: ERROR fname is NULL \n", __FUNCTION__);
		return;
	}

	filep = fopen(logstrs_path, "rb");
	if (!filep) {
		perror(logstrs_path);
		fprintf(stderr, "Cannot open file %s\n", logstrs_path);
		return;
	}

	if (fseek(filep, 0, SEEK_END) < 0 ||
	    (logstrs_size = ftell(filep)) < 0) {
		fprintf(stderr, "%s: Could not determine size of %s \n", __FUNCTION__,
			logstrs_path);
		goto fail;
	}

	raw_fmts = (char *)malloc(logstrs_size);
	if (raw_fmts == NULL) {
		fprintf(stderr, "%s: Failed to allocate memory \n", __FUNCTION__);
		goto fail;
	}

	fseek(filep, 0, SEEK_SET);
	error = fread(raw_fmts, 1, logstrs_size, filep);
	if (error != logstrs_size) {
		fprintf(stderr, "%s: Failed to read file %s", __FUNCTION__, logstrs_path);
		goto fail;
	}

	/* Remember header from the logstrs.bin file */
	hdr = (logstr_header_t *) (raw_fmts + logstrs_size -
			sizeof(logstr_header_t));

	if (hdr->log_magic == LOGSTRS_MAGIC) {
		/*
		 * logstrs.bin start with header.
		 */
		num_fmts = hdr->rom_logstrs_offset / sizeof(uint32);
		ram_index = (hdr->ram_lognums_offset -
			hdr->rom_lognums_offset) / sizeof(uint32);
		lognums = (uint32 *)&raw_fmts[hdr->rom_lognums_offset];
		logstrs = (char *)&raw_fmts[hdr->rom_logstrs_offset];
	} else {
		/*
		 * Legacy logstrs.bin format without header.
		 */
		num_fmts = *((uint32 *) (raw_fmts)) / sizeof(uint32);

		/* Legacy RAM-only logstrs.bin format:
		 *	  - RAM 'lognums' section
		 *	  - RAM 'logstrs' section.
		 *
		 * 'lognums' is an array of indexes for the strings in the
		 * 'logstrs' section. The first uint32 is an index to the
		 * start of 'logstrs'. Therefore, if this index is divided
		 * by 'sizeof(uint32)' it provides the number of logstr
		 *	entries.
		 */
		ram_index = 0;
		lognums = (uint32 *)raw_fmts;
		logstrs = (char *)&raw_fmts[num_fmts << 2];
	}
	if (num_fmts)
		fmts = (char **)malloc(num_fmts * sizeof(char *));
	if (fmts == NULL) {
		fprintf(stderr, "%s: Failed to allocate fmts memory\n", __FUNCTION__);
		goto fail;
	}

	raw_event.fmts_size = num_fmts  * sizeof(char *);

	for (i = 0; i < num_fmts; i++) {
		/* ROM lognums index into logstrs using 'rom_logstrs_offset' as a base
		 * (they are 0-indexed relative to 'rom_logstrs_offset').
		 *
		 * RAM lognums are already indexed to point to the correct RAM logstrs (they
		 * are 0-indexed relative to the start of the logstrs.bin file).
		 */
		if (i == ram_index) {
			logstrs = raw_fmts;
		}
		fmts[i] = &logstrs[lognums[i]];
	}
	raw_event.fmts = fmts;
	raw_event.raw_fmts_size = logstrs_size;
	raw_event.raw_fmts = raw_fmts;
	raw_event.num_fmts = num_fmts;

	fclose(filep);
	return;

fail:
	if (raw_fmts) {
		free(raw_fmts);
	}

	if (filep) {
		fclose(filep);
	}
}

static int wl_init_frm_array(char *dir, char *files)
{
	char *logstrs = NULL, *logstrs_d = NULL;
	char *st_str_file = NULL, *st_str_file_d = NULL;
	char *map_file = NULL, *map_file_d = NULL;
	char *rom_st_str_file = NULL, *rom_st_str_file_d = NULL;
	char *rom_map_file = NULL, *rom_map_file_d = NULL;
	char *next, *str, *eq, *valstr;
	size_t len;

	if ((next = files)) {
		while ((len = strcspn(next, " ,")) > 0) {
			str = next;
			next += len;
			next += strspn(next, " ,");

			str[len] = '\0';
			eq = strchr(str, '=');
			if (eq == NULL) {
				fprintf(stderr, "wl_init_frm_array: missing \" = \" in file "
					"param \"%s\"\n", str);
				return BCME_USAGE_ERROR;
			}
			valstr = eq + 1;
			if (*valstr == '\0') {
				fprintf(stderr, "wl_init_frm_array: missing value after "
					"\" = \" in file param \"%s\"\n", str);
				return BCME_USAGE_ERROR;
			}
			*eq = '\0';

			if (!strcmp(str, "logstrs_path"))
				logstrs = valstr;
			else if (!strcmp(str, "st_str_file_path"))
				st_str_file = valstr;
			else if (!strcmp(str, "map_file_path"))
				map_file = valstr;
			else if (!strcmp(str, "rom_st_str_file_path"))
				rom_st_str_file = valstr;
			else if (!strcmp(str, "rom_map_file_path"))
				rom_map_file = valstr;
			else {
				fprintf(stderr, "wl_init_frm_array: error file %s\n", str);
				return BCME_USAGE_ERROR;
			}
		}
	}

	if (dir) {
		len = strlen(dir);
		if (!logstrs) {
			logstrs_d = (char *)malloc(len + 32);
			logstrs = logstrs_d;
			if (logstrs) {
				strcpy(logstrs, dir);
				strcat(logstrs, "/logstrs.bin");
			}
		}
		if (!st_str_file) {
			st_str_file_d = (char *)malloc(len + 32);
			st_str_file = st_str_file_d;
			if (st_str_file) {
				strcpy(st_str_file, dir);
				strcat(st_str_file, "/rtecdc.bin");
			}
		}
		if (!map_file) {
			map_file_d = (char *)malloc(len + 32);
			map_file = map_file_d;
			if (map_file) {
				strcpy(map_file, dir);
				strcat(map_file, "/rtecdc.map");
			}
		}
		if (!rom_st_str_file) {
			rom_st_str_file_d = (char *)malloc(len + 32);
			rom_st_str_file = rom_st_str_file_d;
			if (rom_st_str_file) {
				strcpy(rom_st_str_file, dir);
				strcat(rom_st_str_file, "/roml.bin");
			}
		}
		if (!rom_map_file) {
			rom_map_file_d = (char *)malloc(len + 32);
			rom_map_file = rom_map_file_d;
			if (rom_map_file) {
				strcpy(rom_map_file, dir);
				strcat(rom_map_file, "/roml.map");
			}
		}
	}

	if (!logstrs)
		logstrs = logstrs_path;
	if (!st_str_file)
		st_str_file = st_str_file_path;
	if (!map_file)
		map_file = map_file_path;
	if (!rom_st_str_file)
		rom_st_str_file = rom_st_str_file_path;
	if (!rom_map_file)
		rom_map_file = rom_map_file_path;

	wl_init_logstrs_array(logstrs);
	wl_init_static_strs_array(st_str_file, map_file);
	wl_init_static_strs_array(rom_st_str_file, rom_map_file);

	if (dir) {
		if (logstrs_d) {
			free(logstrs_d);
		}
		if (st_str_file_d) {
			free(st_str_file_d);
		}
		if (map_file_d) {
			free(map_file_d);
		}
		if (rom_st_str_file_d) {
			free(rom_st_str_file_d);
		}
		if (rom_map_file_d) {
			free(rom_map_file_d);
		}
	}

	return BCME_OK;
}

#define MSCH_EVENTS_PRINT(nbytes) \
	do { \
		printf("%s", mschbufp); \
		if (mschfp) \
			fwrite(mschbufp, 1, nbytes, mschfp); \
	} while (0)

#define MSCH_EVENTS_SPPRINT(space) \
	do { \
		if (space > 0) { \
			int ii; \
			for (ii = 0; ii < space; ii++) mschbufp[ii] = ' '; \
			mschbufp[space] = '\0'; \
			MSCH_EVENTS_PRINT(space); \
		} \
	} while (0)

#define MSCH_EVENTS_PRINTF(fmt) \
	do { \
		int nbytes = snprintf(mschbufp, WL_MSCH_PROFILER_BUFFER_SIZE, fmt); \
		MSCH_EVENTS_PRINT(nbytes); \
	} while (0)

#define MSCH_EVENTS_PRINTF1(fmt, a) \
	do { \
		int nbytes = snprintf(mschbufp, WL_MSCH_PROFILER_BUFFER_SIZE, fmt, \
			(a)); \
		MSCH_EVENTS_PRINT(nbytes); \
	} while (0)

#define MSCH_EVENTS_PRINTF2(fmt, a, b) \
	do { \
		int nbytes = snprintf(mschbufp, WL_MSCH_PROFILER_BUFFER_SIZE, fmt, \
			(a), (b)); \
		MSCH_EVENTS_PRINT(nbytes); \
	} while (0)

#define MSCH_EVENTS_PRINTF3(fmt, a, b, c) \
	do { \
		int nbytes = snprintf(mschbufp, WL_MSCH_PROFILER_BUFFER_SIZE, fmt, \
			(a), (b), (c)); \
		MSCH_EVENTS_PRINT(nbytes); \
	} while (0)

#define MSCH_EVENTS_PRINTF4(fmt, a, b, c, d) \
	do { \
		int nbytes = snprintf(mschbufp, WL_MSCH_PROFILER_BUFFER_SIZE, fmt, \
			(a), (b), (c), (d)); \
		MSCH_EVENTS_PRINT(nbytes); \
	} while (0)

#define MSCH_EVENTS_PRINTF5(fmt, a, b, c, d, e) \
	do { \
		int nbytes = snprintf(mschbufp, WL_MSCH_PROFILER_BUFFER_SIZE, fmt, \
			(a), (b), (c), (d), (e)); \
		MSCH_EVENTS_PRINT(nbytes); \
	} while (0)

#define MSCH_EVENTS_SPPRINTF(space, fmt) \
	do { \
		MSCH_EVENTS_SPPRINT(space); \
		MSCH_EVENTS_PRINTF(fmt); \
	} while (0)

#define MSCH_EVENTS_SPPRINTF1(space, fmt, a) \
	do { \
		MSCH_EVENTS_SPPRINT(space); \
		MSCH_EVENTS_PRINTF1(fmt, (a)); \
	} while (0)

#define MSCH_EVENTS_SPPRINTF2(space, fmt, a, b) \
	do { \
		MSCH_EVENTS_SPPRINT(space); \
		MSCH_EVENTS_PRINTF2(fmt, (a), (b)); \
	} while (0)

#define MSCH_EVENTS_SPPRINTF3(space, fmt, a, b, c) \
	do { \
		MSCH_EVENTS_SPPRINT(space); \
		MSCH_EVENTS_PRINTF3(fmt, (a), (b), (c)); \
	} while (0)

#define MSCH_EVENTS_SPPRINTF4(space, fmt, a, b, c, d) \
	do { \
		MSCH_EVENTS_SPPRINT(space); \
		MSCH_EVENTS_PRINTF4(fmt, (a), (b), (c), (d)); \
	} while (0)

#define MSCH_EVENTS_SPPRINTF5(space, fmt, a, b, c, d, e) \
	do { \
		MSCH_EVENTS_SPPRINT(space); \
		MSCH_EVENTS_PRINTF5(fmt, (a), (b), (c), (d), (e)); \
	} while (0)

static char *wl_msch_display_time(uint32 time_h, uint32 time_l)
{
	static char display_time[32];
	uint64 t;
	uint32 s, ss;

	if (time_h == 0xffffffff && time_l == 0xffffffff) {
		snprintf(display_time, 31, "-1");
	} else {
		t = ((uint64)(ntoh32(time_h)) << 32) | ntoh32(time_l);
		s = (uint32)(t / 1000000);
		ss = (uint32)(t % 1000000);
		snprintf(display_time, 31, "%d.%06d", s, ss);
	}
	return display_time;
}

static void
wl_msch_chanspec_list(int sp, char *data, uint16 ptr, uint16 chanspec_cnt)
{
	int i, cnt = (int)ntoh16(chanspec_cnt);
	uint16 *chanspec_list = (uint16 *)(data + ntoh16(ptr));
	char buf[CHANSPEC_STR_LEN];
	chanspec_t c;

	MSCH_EVENTS_SPPRINTF(sp, "<chanspec_list>:");
	for (i = 0; i < cnt; i++) {
		c = (chanspec_t)ntoh16(chanspec_list[i]);
		MSCH_EVENTS_PRINTF1(" %s", wf_chspec_ntoa(c, buf));
	}
	MSCH_EVENTS_PRINTF("\n");
}

static void
wl_msch_elem_list(int sp, char *title, char *data, uint16 ptr, uint16 list_cnt)
{
	int i, cnt = (int)ntoh16(list_cnt);
	uint32 *list = (uint32 *)(data + ntoh16(ptr));

	MSCH_EVENTS_SPPRINTF1(sp, "%s_list: ", title);
	for (i = 0; i < cnt; i++) {
		MSCH_EVENTS_PRINTF1("0x%08x->", ntoh32(list[i]));
	}
	MSCH_EVENTS_PRINTF("null\n");
}

static void
wl_msch_req_param_profiler_data(int sp, int ver, char *data, uint16 ptr)
{
	int sn = sp + 4;
	msch_req_param_profiler_event_data_t *p =
		(msch_req_param_profiler_event_data_t *)(data + ntoh16(ptr));
	uint32 type, flags;

	UNUSED_PARAMETER(ver);

	MSCH_EVENTS_SPPRINTF(sp, "<request parameters>\n");
	MSCH_EVENTS_SPPRINTF(sn, "req_type: ");

	type = p->req_type;
	if (type < 4) {
		char *req_type[] = {"fixed", "start-flexible", "duration-flexible",
			"both-flexible"};
		MSCH_EVENTS_PRINTF1("%s", req_type[type]);
	}
	else
		MSCH_EVENTS_PRINTF1("unknown(%d)", type);

	flags = ntoh16(p->flags);
	if (flags & WL_MSCH_REQ_FLAGS_CHAN_CONTIGUOUS)
		MSCH_EVENTS_PRINTF(", CHAN_CONTIGUOUS");
	if (flags & WL_MSCH_REQ_FLAGS_MERGE_CONT_SLOTS)
		MSCH_EVENTS_PRINTF(", MERGE_CONT_SLOTS");
	if (flags & WL_MSCH_REQ_FLAGS_PREMTABLE)
		MSCH_EVENTS_PRINTF(", PREMTABLE");
	if (flags & WL_MSCH_REQ_FLAGS_PREMT_CURTS)
		MSCH_EVENTS_PRINTF(", PREMT_CURTS");
	if (flags & WL_MSCH_REQ_FLAGS_PREMT_IMMEDIATE)
		MSCH_EVENTS_PRINTF(", PREMT_IMMEDIATE");
	MSCH_EVENTS_PRINTF1(", priority: %d\n", p->priority);

	MSCH_EVENTS_SPPRINTF3(sn, "start-time: %s, duration: %d(us), interval: %d(us)\n",
		wl_msch_display_time(p->start_time_h, p->start_time_l),
		ntoh32(p->duration), ntoh32(p->interval));

	if (type == WL_MSCH_RT_DUR_FLEX)
		MSCH_EVENTS_SPPRINTF1(sn, "dur_flex: %d(us)\n", ntoh32(p->flex.dur_flex));
	else if (type == WL_MSCH_RT_BOTH_FLEX) {
		MSCH_EVENTS_SPPRINTF2(sn, "min_dur: %d(us), max_away_dur: %d(us)\n",
			ntoh32(p->flex.bf.min_dur), ntoh32(p->flex.bf.max_away_dur));

		MSCH_EVENTS_SPPRINTF2(sn, "hi_prio_time: %s, hi_prio_interval: %d(us)\n",
			wl_msch_display_time(p->flex.bf.hi_prio_time_h,
			p->flex.bf.hi_prio_time_l),
			ntoh32(p->flex.bf.hi_prio_interval));
	}
}

static void
wl_msch_timeslot_profiler_data(int sp, int ver, char *title, char *data, uint16 ptr, bool empty)
{
	int s, sn = sp + 4;
	msch_timeslot_profiler_event_data_t *p =
		(msch_timeslot_profiler_event_data_t *)(data + ntoh16(ptr));
	char *state[] = {"NONE", "CHN_SW", "ONCHAN_FIRE", "OFF_CHN_PREP",
		"OFF_CHN_DONE", "TS_COMPLETE"};

	UNUSED_PARAMETER(ver);

	MSCH_EVENTS_SPPRINTF1(sp, "<%s timeslot>: ", title);
	if (empty) {
		MSCH_EVENTS_PRINTF(" null\n");
		return;
	}
	else
		MSCH_EVENTS_PRINTF1("0x%08x\n", ntoh32(p->p_timeslot));

	s = (int)(ntoh32(p->state));
	if (s > 5) s = 0;

	MSCH_EVENTS_SPPRINTF4(sn, "id: %d, state[%d]: %s, chan_ctxt: [0x%08x]\n",
		ntoh32(p->timeslot_id), ntoh32(p->state), state[s], ntoh32(p->p_chan_ctxt));

	MSCH_EVENTS_SPPRINTF1(sn, "fire_time: %s",
		wl_msch_display_time(p->fire_time_h, p->fire_time_l));

	MSCH_EVENTS_PRINTF1(", pre_start_time: %s",
		wl_msch_display_time(p->pre_start_time_h, p->pre_start_time_l));

	MSCH_EVENTS_PRINTF1(", end_time: %s",
		wl_msch_display_time(p->end_time_h, p->end_time_l));

	MSCH_EVENTS_PRINTF1(", sch_dur: %s\n",
		wl_msch_display_time(p->sch_dur_h, p->sch_dur_l));
}

static void
wl_msch_req_timing_profiler_data(int sp, int ver, char *title, char *data, uint16 ptr, bool empty)
{
	int sn = sp + 4;
	msch_req_timing_profiler_event_data_t *p =
		(msch_req_timing_profiler_event_data_t *)(data + ntoh16(ptr));
	uint32 type;

	UNUSED_PARAMETER(ver);

	MSCH_EVENTS_SPPRINTF1(sp, "<%s req_timing>: ", title);
	if (empty) {
		MSCH_EVENTS_PRINTF(" null\n");
		return;
	}
	else
		MSCH_EVENTS_PRINTF3("0x%08x (prev 0x%08x, next 0x%08x)\n",
			ntoh32(p->p_req_timing), ntoh32(p->p_prev), ntoh32(p->p_next));

	MSCH_EVENTS_SPPRINTF(sn, "flags:");
	type = ntoh16(p->flags);
	if ((type & 0x7f) == 0)
		MSCH_EVENTS_PRINTF(" NONE");
	else {
		if (type & WL_MSCH_RC_FLAGS_ONCHAN_FIRE)
			MSCH_EVENTS_PRINTF(" ONCHAN_FIRE");
		if (type & WL_MSCH_RC_FLAGS_START_FIRE_DONE)
			MSCH_EVENTS_PRINTF(" START_FIRE");
		if (type & WL_MSCH_RC_FLAGS_END_FIRE_DONE)
			MSCH_EVENTS_PRINTF(" END_FIRE");
		if (type & WL_MSCH_RC_FLAGS_ONFIRE_DONE)
			MSCH_EVENTS_PRINTF(" ONFIRE_DONE");
		if (type & WL_MSCH_RC_FLAGS_SPLIT_SLOT_START)
			MSCH_EVENTS_PRINTF(" SPLIT_SLOT_START");
		if (type & WL_MSCH_RC_FLAGS_SPLIT_SLOT_END)
			MSCH_EVENTS_PRINTF(" SPLIT_SLOT_END");
		if (type & WL_MSCH_RC_FLAGS_PRE_ONFIRE_DONE)
			MSCH_EVENTS_PRINTF(" PRE_ONFIRE_DONE");
	}
	MSCH_EVENTS_PRINTF("\n");

	MSCH_EVENTS_SPPRINTF1(sn, "pre_start_time: %s",
		wl_msch_display_time(p->pre_start_time_h, p->pre_start_time_l));

	MSCH_EVENTS_PRINTF1(", start_time: %s",
		wl_msch_display_time(p->start_time_h, p->start_time_l));

	MSCH_EVENTS_PRINTF1(", end_time: %s\n",
		wl_msch_display_time(p->end_time_h, p->end_time_l));

	if (p->p_timeslot && (p->timeslot_ptr == 0))
		MSCH_EVENTS_SPPRINTF2(sn, "<%s timeslot>: 0x%08x\n", title, ntoh32(p->p_timeslot));
	else
		wl_msch_timeslot_profiler_data(sn, ver, title, data, p->timeslot_ptr,
			(p->timeslot_ptr == 0));
}

static void
wl_msch_chan_ctxt_profiler_data(int sp, int ver, char *data, uint16 ptr, bool empty)
{
	int sn = sp + 4;
	msch_chan_ctxt_profiler_event_data_t *p =
		(msch_chan_ctxt_profiler_event_data_t *)(data + ntoh16(ptr));
	chanspec_t c;
	char buf[CHANSPEC_STR_LEN];

	UNUSED_PARAMETER(ver);

	MSCH_EVENTS_SPPRINTF(sp, "<chan_ctxt>: ");
	if (empty) {
		MSCH_EVENTS_PRINTF(" null\n");
		return;
	}
	else
		MSCH_EVENTS_PRINTF3("0x%08x (prev 0x%08x, next 0x%08x)\n",
			ntoh32(p->p_chan_ctxt), ntoh32(p->p_prev), ntoh32(p->p_next));

	c = (chanspec_t)ntoh16(p->chanspec);
	MSCH_EVENTS_SPPRINTF3(sn, "channel: %s, bf_sch_pending: %s, bf_skipped: %d\n",
		wf_chspec_ntoa(c, buf), p->bf_sch_pending? "TRUE" : "FALSE",
		ntoh32(p->bf_skipped_count));
	MSCH_EVENTS_SPPRINTF2(sn, "bf_link: prev 0x%08x, next 0x%08x\n",
		ntoh32(p->bf_link_prev), ntoh32(p->bf_link_next));

	MSCH_EVENTS_SPPRINTF1(sn, "onchan_time: %s",
		wl_msch_display_time(p->onchan_time_h, p->onchan_time_l));

	MSCH_EVENTS_PRINTF1(", actual_onchan_dur: %s",
		wl_msch_display_time(p->actual_onchan_dur_h, p->actual_onchan_dur_l));

	MSCH_EVENTS_PRINTF1(", pend_onchan_dur: %s\n",
		wl_msch_display_time(p->pend_onchan_dur_h, p->pend_onchan_dur_l));

	wl_msch_elem_list(sn, "req_entity", data, p->req_entity_list_ptr, p->req_entity_list_cnt);
	wl_msch_elem_list(sn, "bf_entity", data, p->bf_entity_list_ptr, p->bf_entity_list_cnt);
}

static void
wl_msch_req_entity_profiler_data(int sp, int ver, char *data, uint16 ptr, bool empty)
{
	int sn = sp + 4;
	msch_req_entity_profiler_event_data_t *p =
		(msch_req_entity_profiler_event_data_t *)(data + ntoh16(ptr));
	char buf[CHANSPEC_STR_LEN];
	chanspec_t c;
	uint32 flags;

	MSCH_EVENTS_SPPRINTF(sp, "<req_entity>: ");
	if (empty) {
		MSCH_EVENTS_PRINTF(" null\n");
		return;
	}
	else
		MSCH_EVENTS_PRINTF3("0x%08x (prev 0x%08x, next 0x%08x)\n",
			ntoh32(p->p_req_entity), ntoh32(p->req_hdl_link_prev),
			ntoh32(p->req_hdl_link_next));

	MSCH_EVENTS_SPPRINTF1(sn, "req_hdl: [0x%08x]\n", ntoh32(p->p_req_hdl));
	MSCH_EVENTS_SPPRINTF2(sn, "chan_ctxt_link: prev 0x%08x, next 0x%08x\n",
		ntoh32(p->chan_ctxt_link_prev), ntoh32(p->chan_ctxt_link_next));
	MSCH_EVENTS_SPPRINTF2(sn, "rt_specific_link: prev 0x%08x, next 0x%08x\n",
		ntoh32(p->rt_specific_link_prev), ntoh32(p->rt_specific_link_next));
	MSCH_EVENTS_SPPRINTF2(sn, "start_fixed_link: prev 0x%08x, next 0x%08x\n",
		ntoh32(p->start_fixed_link_prev), ntoh32(p->start_fixed_link_next));
	MSCH_EVENTS_SPPRINTF2(sn, "both_flex_list: prev 0x%08x, next 0x%08x\n",
		ntoh32(p->both_flex_list_prev), ntoh32(p->both_flex_list_next));

	c = (chanspec_t)ntoh16(p->chanspec);
	if (ver >= 2) {
		MSCH_EVENTS_SPPRINTF4(sn, "channel: %s, onchan Id %d, current chan Id %d, "
			"priority %d", wf_chspec_ntoa(c, buf), ntoh16(p->onchan_chn_idx),
			ntoh16(p->cur_chn_idx), ntoh16(p->priority));
		flags = ntoh32(p->flags);
		if (flags & WL_MSCH_ENTITY_FLAG_MULTI_INSTANCE)
			MSCH_EVENTS_PRINTF(" : MULTI_INSTANCE\n");
		else
			MSCH_EVENTS_PRINTF("\n");
		MSCH_EVENTS_SPPRINTF1(sn, "actual_start_time: %s, ",
			wl_msch_display_time(p->actual_start_time_h, p->actual_start_time_l));
		MSCH_EVENTS_PRINTF1("curts_fire_time: %s, ",
			wl_msch_display_time(p->curts_fire_time_h, p->curts_fire_time_l));
	} else {
		MSCH_EVENTS_SPPRINTF2(sn, "channel: %s, priority %d, ", wf_chspec_ntoa(c, buf),
			ntoh16(p->priority));
	}
	MSCH_EVENTS_PRINTF1("bf_last_serv_time: %s\n",
		wl_msch_display_time(p->bf_last_serv_time_h, p->bf_last_serv_time_l));

	wl_msch_req_timing_profiler_data(sn, ver, "current", data, p->cur_slot_ptr,
		(p->cur_slot_ptr == 0));
	wl_msch_req_timing_profiler_data(sn, ver, "pending", data, p->pend_slot_ptr,
		(p->pend_slot_ptr == 0));

	if (p->p_chan_ctxt && (p->chan_ctxt_ptr == 0))
		MSCH_EVENTS_SPPRINTF1(sn, "<chan_ctxt>: 0x%08x\n", ntoh32(p->p_chan_ctxt));
	else
		wl_msch_chan_ctxt_profiler_data(sn, ver, data, p->chan_ctxt_ptr,
			(p->chan_ctxt_ptr == 0));
}

static void
wl_msch_req_handle_profiler_data(int sp, int ver, char *data, uint16 ptr, bool empty)
{
	int sn = sp + 4;
	msch_req_handle_profiler_event_data_t *p =
		(msch_req_handle_profiler_event_data_t *)(data + ntoh16(ptr));
	uint32 flags;

	MSCH_EVENTS_SPPRINTF(sp, "<req_handle>: ");
	if (empty) {
		MSCH_EVENTS_PRINTF(" null\n");
		return;
	}
	else
		MSCH_EVENTS_PRINTF3("0x%08x (prev 0x%08x, next 0x%08x)\n",
			ntoh32(p->p_req_handle), ntoh32(p->p_prev), ntoh32(p->p_next));

	wl_msch_elem_list(sn, "req_entity", data, p->req_entity_list_ptr, p->req_entity_list_cnt);
	MSCH_EVENTS_SPPRINTF2(sn, "cb_func: [0x%08x], cb_func: [0x%08x]",
		ntoh32(p->cb_func), ntoh32(p->cb_ctxt));
	if (ver < 2) {
		MSCH_EVENTS_PRINTF1(", chan_cnt: %d", ntoh16(p->chan_cnt));
	}
	flags = ntoh32(p->flags);
	if (flags & WL_MSCH_REQ_HDL_FLAGS_NEW_REQ)
		MSCH_EVENTS_PRINTF(", NEW_REQ");
	MSCH_EVENTS_PRINTF("\n");

	wl_msch_req_param_profiler_data(sn, ver, data, p->req_param_ptr);

	if (ver >= 2) {
		MSCH_EVENTS_SPPRINTF1(sn, "req_time: %s\n",
			wl_msch_display_time(p->req_time_h, p->req_time_l));
		MSCH_EVENTS_SPPRINTF3(sn, "chan_cnt: %d, chan idx %d, last chan idx %d\n",
			ntoh16(p->chan_cnt), ntoh16(p->chan_idx), ntoh16(p->last_chan_idx));
		if (p->chanspec_list && p->chanspec_cnt) {
			wl_msch_chanspec_list(sn, data, p->chanspec_list, p->chanspec_cnt);
		}
	}
}

static void
wl_msch_profiler_profiler_data(int sp, int ver, char *data, uint16 ptr)
{
	msch_profiler_profiler_event_data_t *p =
		(msch_profiler_profiler_event_data_t *)(data + ntoh16(ptr));
	uint32 flags;

	MSCH_EVENTS_SPPRINTF4(sp, "free list: req_hdl 0x%08x, req_entity 0x%08x,"
		" chan_ctxt 0x%08x, chanspec 0x%08x\n",
		ntoh32(p->free_req_hdl_list), ntoh32(p->free_req_entity_list),
		ntoh32(p->free_chan_ctxt_list), ntoh32(p->free_chanspec_list));

	MSCH_EVENTS_SPPRINTF5(sp, "alloc count: chanspec %d, req_entity %d, req_hdl %d, "
		"chan_ctxt %d, timeslot %d\n",
		ntoh16(p->msch_chanspec_alloc_cnt), ntoh16(p->msch_req_entity_alloc_cnt),
		ntoh16(p->msch_req_hdl_alloc_cnt), ntoh16(p->msch_chan_ctxt_alloc_cnt),
		ntoh16(p->msch_timeslot_alloc_cnt));

	wl_msch_elem_list(sp, "req_hdl", data, p->msch_req_hdl_list_ptr,
		p->msch_req_hdl_list_cnt);
	wl_msch_elem_list(sp, "chan_ctxt", data, p->msch_chan_ctxt_list_ptr,
		p->msch_chan_ctxt_list_cnt);
	wl_msch_elem_list(sp, "req_timing", data, p->msch_req_timing_list_ptr,
		p->msch_req_timing_list_cnt);
	wl_msch_elem_list(sp, "start_fixed", data, p->msch_start_fixed_list_ptr,
		p->msch_start_fixed_list_cnt);
	wl_msch_elem_list(sp, "both_flex_req_entity", data,
		p->msch_both_flex_req_entity_list_ptr,
		p->msch_both_flex_req_entity_list_cnt);
	wl_msch_elem_list(sp, "start_flex", data, p->msch_start_flex_list_ptr,
		p->msch_start_flex_list_cnt);
	wl_msch_elem_list(sp, "both_flex", data, p->msch_both_flex_list_ptr,
		p->msch_both_flex_list_cnt);

	if (p->p_cur_msch_timeslot && (p->cur_msch_timeslot_ptr == 0))
		MSCH_EVENTS_SPPRINTF1(sp, "<cur_msch timeslot>: 0x%08x\n",
			ntoh32(p->p_cur_msch_timeslot));
	else
		wl_msch_timeslot_profiler_data(sp, ver, "cur_msch", data,
			p->cur_msch_timeslot_ptr, (p->cur_msch_timeslot_ptr == 0));

	if (p->p_next_timeslot && (p->next_timeslot_ptr == 0))
		MSCH_EVENTS_SPPRINTF1(sp, "<next timeslot>: 0x%08x\n", ntoh32(p->p_next_timeslot));
	else
		wl_msch_timeslot_profiler_data(sp, ver, "next", data,
			p->next_timeslot_ptr, (p->next_timeslot_ptr == 0));

	MSCH_EVENTS_SPPRINTF1(sp, "ts_id: %d, ", ntoh32(p->ts_id));
	flags = ntoh32(p->flags);
	if (flags & WL_MSCH_STATE_IN_TIEMR_CTXT)
		MSCH_EVENTS_PRINTF("IN_TIEMR_CTXT, ");
	if (flags & WL_MSCH_STATE_SCHD_PENDING)
		MSCH_EVENTS_PRINTF("SCHD_PENDING, ");
	MSCH_EVENTS_PRINTF2("slotskip_flags: %d, cur_armed_timeslot: 0x%08x\n",
		(ver >= 2)? ntoh32(p->slotskip_flag) : 0, ntoh32(p->cur_armed_timeslot));
	MSCH_EVENTS_SPPRINTF3(sp, "flex_list_cnt: %d, service_interval: %d, "
		"max_lo_prio_interval: %d\n",
		ntoh16(p->flex_list_cnt), ntoh32(p->service_interval),
		ntoh32(p->max_lo_prio_interval));
}

#define MAX_NO_OF_ARG	10
#define FMTSTR_SIZE	132
#define ROMSTR_SIZE	200
#define SIZE_LOC_STR	50

static bool
check_valid_string_format(char *curr_ptr)
{
	char *next_ptr;
	if ((next_ptr = bcmstrstr(curr_ptr, "s")) != NULL) {
		/* Default %s format */
		if (curr_ptr == next_ptr) {
			return TRUE;
		}

		/* Verify each charater between '%' and 's' is a valid number */
		while (curr_ptr < next_ptr) {
			if (bcm_isdigit(*curr_ptr) == FALSE) {
				return FALSE;
			}
			curr_ptr++;
		}

		return TRUE;
	} else {
		return FALSE;
	}
}

static void
wl_msch_profiler_event_log_data(int ver, event_log_hdr_t *hdr, uint32 *data)
{
	uint16 count;
	char fmtstr_loc_buf[ROMSTR_SIZE] = { 0 };
	char (*str_buf)[SIZE_LOC_STR] = NULL;
	char *str_tmpptr = NULL;
	uint32 addr = 0;
	typedef union {
		uint32 val;
		char * addr;
	} u_arg;
	u_arg arg[MAX_NO_OF_ARG] = {{0}};
	char *c_ptr = NULL;
	int nbytes;

	UNUSED_PARAMETER(ver);

	/* print the message out in a logprint	*/
	if (!(((raw_event.raw_sstr) || (raw_event.rom_raw_sstr)) &&
		raw_event.fmts) || hdr->fmt_num == 0xffff) {
		MSCH_EVENTS_PRINTF2("0.0 EL: %x %x",
			hdr->tag & EVENT_LOG_TAG_FLAG_SET_MASK,
			hdr->fmt_num);
		for (count = 0; count < hdr->count; count++)
			MSCH_EVENTS_PRINTF1(" %x", data[count]);
		MSCH_EVENTS_PRINTF("\n");
		return;
	}

	str_buf = malloc(MAX_NO_OF_ARG * SIZE_LOC_STR);
	if (!str_buf) {
		fprintf(stderr, "%s: malloc failed str_buf\n", __FUNCTION__);
		return;
	}

	if ((hdr->fmt_num >> 2) < raw_event.num_fmts) {
		snprintf(fmtstr_loc_buf, FMTSTR_SIZE, "%s",
				raw_event.fmts[hdr->fmt_num >> 2]);
		c_ptr = fmtstr_loc_buf;
	} else {
		fprintf(stderr, "%s: fmt number out of range \n", __FUNCTION__);
		goto exit;
	}

	for (count = 0; count < hdr->count; count++) {
		if (c_ptr != NULL)
			if ((c_ptr = bcmstrstr(c_ptr, "%")) != NULL)
				c_ptr++;

		if (c_ptr != NULL) {
			if (check_valid_string_format(c_ptr)) {
				if ((raw_event.raw_sstr) &&
					((data[count] > raw_event.rodata_start) &&
					(data[count] < raw_event.rodata_end))) {
					/* ram static string */
					addr = data[count] - raw_event.rodata_start;
					str_tmpptr = raw_event.raw_sstr + addr;
					memcpy(str_buf[count], str_tmpptr,
						SIZE_LOC_STR);
					str_buf[count][SIZE_LOC_STR-1] = '\0';
					arg[count].addr = str_buf[count];
				} else if ((raw_event.rom_raw_sstr) &&
						((data[count] >
						raw_event.rom_rodata_start) &&
						(data[count] <
						raw_event.rom_rodata_end))) {
					/* rom static string */
					addr = data[count] - raw_event.rom_rodata_start;
					str_tmpptr = raw_event.rom_raw_sstr + addr;
					memcpy(str_buf[count], str_tmpptr,
						SIZE_LOC_STR);
					str_buf[count][SIZE_LOC_STR-1] = '\0';
					arg[count].addr = str_buf[count];
				} else {
					/*
					 *  Dynamic string OR
					 * No data for static string.
					 * So store all string's address as string.
					 */
					snprintf(str_buf[count], SIZE_LOC_STR,
						"(s)0x%x", data[count]);
					arg[count].addr = str_buf[count];
				}
			} else {
				/* Other than string */
				arg[count].val = data[count];
			}
		}
	}

	nbytes = snprintf(mschbufp, WL_MSCH_PROFILER_BUFFER_SIZE, fmtstr_loc_buf,
		arg[0], arg[1], arg[2], arg[3], arg[4], arg[5], arg[6], arg[7], arg[8], arg[9]);
	MSCH_EVENTS_PRINT(nbytes);
exit:
	free(str_buf);
}

static uint64 solt_start_time[4], req_start_time[4], profiler_start_time[4];
static uint32 solt_chanspec[4] = {0, }, req_start[4] = {0, };
static bool lastMessages = FALSE;

static void wl_msch_dump_data(char *data, int type)
{
	uint64 t = 0, tt = 0;
	uint32 s = 0, ss = 0;
	int wlc_index, ver;

	ver = (type & WL_MSCH_PROFILER_VER_MASK) >> WL_MSCH_PROFILER_VER_SHIFT;
	wlc_index = (type & WL_MSCH_PROFILER_WLINDEX_MASK) >> WL_MSCH_PROFILER_WLINDEX_SHIFT;
	if (wlc_index >= 4)
		return;

	type &= WL_MSCH_PROFILER_TYPE_MASK;
	if (type <= WL_MSCH_PROFILER_PROFILE_END || type == WL_MSCH_PROFILER_EVENT_LOG) {
		msch_profiler_event_data_t *pevent = (msch_profiler_event_data_t *)data;
		tt = ((uint64)(ntoh32(pevent->time_hi)) << 32) | ntoh32(pevent->time_lo);
		s = (uint32)(tt / 1000000);
		ss = (uint32)(tt % 1000000);
	}

	if (lastMessages && (type != WL_MSCH_PROFILER_MESSAGE) &&
		(type != WL_MSCH_PROFILER_EVENT_LOG)) {
		MSCH_EVENTS_PRINTF("\n");
		lastMessages = FALSE;
	}

	switch (type) {
	case WL_MSCH_PROFILER_START:
		MSCH_EVENTS_PRINTF2("\n%06d.%06d START\n", s, ss);
		break;

	case WL_MSCH_PROFILER_EXIT:
		MSCH_EVENTS_PRINTF2("\n%06d.%06d EXIT\n", s, ss);
		break;

	case WL_MSCH_PROFILER_REQ:
	{
		msch_req_profiler_event_data_t *p = (msch_req_profiler_event_data_t *)data;
		MSCH_EVENTS_PRINTF("\n===============================\n");
		MSCH_EVENTS_PRINTF3("%06d.%06d [wl%d] REGISTER:\n", s, ss, wlc_index);
		wl_msch_req_param_profiler_data(4, ver, data, p->req_param_ptr);
		wl_msch_chanspec_list(4, data, p->chanspec_ptr, p->chanspec_cnt);
		MSCH_EVENTS_PRINTF("===============================\n\n");
	}
		break;

	case WL_MSCH_PROFILER_CALLBACK:
	{
		msch_callback_profiler_event_data_t *p =
			(msch_callback_profiler_event_data_t *)data;
		char buf[CHANSPEC_STR_LEN];
		uint16 cbtype;

		MSCH_EVENTS_PRINTF3("%06d.%06d [wl%d] CALLBACK: ", s, ss, wlc_index);
		ss = ntoh16(p->chanspec);
		if (ver >= 2) {
			MSCH_EVENTS_PRINTF2("req_hdl[0x%08x], channel %s --",
				ntoh32(p->p_req_hdl), wf_chspec_ntoa(ss, buf));
		} else {
			MSCH_EVENTS_PRINTF1("channel %s --", wf_chspec_ntoa(ss, buf));
		}
		cbtype = ntoh16(p->type);
		if (cbtype & WL_MSCH_CT_ON_CHAN)
			MSCH_EVENTS_PRINTF(" ON_CHAN");
		if (cbtype & WL_MSCH_CT_OFF_CHAN)
			MSCH_EVENTS_PRINTF(" OFF_CHAN");
		if (cbtype & WL_MSCH_CT_REQ_START)
			MSCH_EVENTS_PRINTF(" REQ_START");
		if (cbtype & WL_MSCH_CT_REQ_END)
			MSCH_EVENTS_PRINTF(" REQ_END");
		if (cbtype & WL_MSCH_CT_SLOT_START)
			MSCH_EVENTS_PRINTF(" SLOT_START");
		if (cbtype & WL_MSCH_CT_SLOT_SKIP)
			MSCH_EVENTS_PRINTF(" SLOT_SKIP");
		if (cbtype & WL_MSCH_CT_SLOT_END)
			MSCH_EVENTS_PRINTF(" SLOT_END");
		if (cbtype & WL_MSCH_CT_OFF_CHAN_DONE)
			MSCH_EVENTS_PRINTF(" OFF_CHAN_DONE");
		if (cbtype & WL_MSCH_CT_PARTIAL)
			MSCH_EVENTS_PRINTF(" PARTIAL");
		if (cbtype & WL_MSCH_CT_PRE_ONCHAN)
			MSCH_EVENTS_PRINTF(" PRE_ONCHAN");
		if (cbtype & WL_MSCH_CT_PRE_REQ_START)
			MSCH_EVENTS_PRINTF(" PRE_REQ_START");

		if (cbtype & (WL_MSCH_CT_ON_CHAN | WL_MSCH_CT_SLOT_SKIP)) {
			MSCH_EVENTS_PRINTF("\n    ");
			if (cbtype & WL_MSCH_CT_ON_CHAN) {
				if (ver >= 2) {
					MSCH_EVENTS_PRINTF3("ID %d onchan idx %d seq_start %s ",
						ntoh32(p->timeslot_id), ntoh32(p->onchan_idx),
						wl_msch_display_time(p->cur_chan_seq_start_time_h,
						p->cur_chan_seq_start_time_l));
				} else {
					MSCH_EVENTS_PRINTF1("ID %d ", ntoh32(p->timeslot_id));
				}
			}
			t = ((uint64)(ntoh32(p->start_time_h)) << 32) |
				ntoh32(p->start_time_l);
			MSCH_EVENTS_PRINTF1("start %s ",
				wl_msch_display_time(p->start_time_h,
				p->start_time_l));
			tt = ((uint64)(ntoh32(p->end_time_h)) << 32) | ntoh32(p->end_time_l);
			MSCH_EVENTS_PRINTF2("end %s duration %d",
				wl_msch_display_time(p->end_time_h, p->end_time_l),
				(p->end_time_h == 0xffffffff && p->end_time_l == 0xffffffff)?
				-1 : (int)(tt - t));
		}

		if (cbtype & WL_MSCH_CT_REQ_START) {
			req_start[wlc_index] = 1;
			req_start_time[wlc_index] = tt;
		} else if (cbtype & WL_MSCH_CT_REQ_END) {
			if (req_start[wlc_index]) {
				MSCH_EVENTS_PRINTF1(" : REQ duration %d",
					(uint32)(tt - req_start_time[wlc_index]));
				req_start[wlc_index] = 0;
			}
		}

		if (cbtype & WL_MSCH_CT_SLOT_START) {
			solt_chanspec[wlc_index] = p->chanspec;
			solt_start_time[wlc_index] = tt;
		} else if (cbtype & WL_MSCH_CT_SLOT_END) {
			if (p->chanspec == solt_chanspec[wlc_index]) {
				MSCH_EVENTS_PRINTF1(" : SLOT duration %d",
					(uint32)(tt - solt_start_time[wlc_index]));
				solt_chanspec[wlc_index] = 0;
			}
		}
		MSCH_EVENTS_PRINTF("\n");
	}
		break;

	case WL_MSCH_PROFILER_EVENT_LOG:
	{
		msch_event_log_profiler_event_data_t *p =
			(msch_event_log_profiler_event_data_t *)data;
		p->hdr.fmt_num = ntoh16(p->hdr.fmt_num);
		MSCH_EVENTS_PRINTF3("%06d.%06d [wl%d]: ", s, ss, wlc_index);
		wl_msch_profiler_event_log_data(ver, &p->hdr, p->data);
		lastMessages = TRUE;
		break;
	}

	case WL_MSCH_PROFILER_MESSAGE:
	{
		msch_message_profiler_event_data_t *p = (msch_message_profiler_event_data_t *)data;
		MSCH_EVENTS_PRINTF4("%06d.%06d [wl%d]: %s", s, ss, wlc_index, p->message);
		lastMessages = TRUE;
		break;
	}

	case WL_MSCH_PROFILER_PROFILE_START:
		profiler_start_time[wlc_index] = tt;
		MSCH_EVENTS_PRINTF("-------------------------------\n");
		MSCH_EVENTS_PRINTF3("%06d.%06d [wl%d] PROFILE DATA:\n", s, ss, wlc_index);
		wl_msch_profiler_profiler_data(4, ver, data, 0);
		break;

	case WL_MSCH_PROFILER_PROFILE_END:
		MSCH_EVENTS_PRINTF4("%06d.%06d [wl%d] PROFILE END: take time %d\n", s, ss,
			wlc_index, (uint32)(tt - profiler_start_time[wlc_index]));
		MSCH_EVENTS_PRINTF("-------------------------------\n\n");
		break;

	case WL_MSCH_PROFILER_REQ_HANDLE:
		wl_msch_req_handle_profiler_data(4, ver, data, 0, FALSE);
		break;

	case WL_MSCH_PROFILER_REQ_ENTITY:
		wl_msch_req_entity_profiler_data(4, ver, data, 0, FALSE);
		break;

	case WL_MSCH_PROFILER_CHAN_CTXT:
		wl_msch_chan_ctxt_profiler_data(4, ver, data, 0, FALSE);
		break;

	case WL_MSCH_PROFILER_REQ_TIMING:
		wl_msch_req_timing_profiler_data(4, ver, "msch", data, 0, FALSE);
		break;

	default:
		fprintf(stderr, "[wl%d] ERROR: unsupported EVENT reason code:%d; ",
			wlc_index, type);
		break;
	}
}

int wl_msch_dump(void *wl, cmd_t *cmd, char **argv)
{
	void *ptr;
	int type;
	char *data, *p, opt;
	char *fname = NULL, *dir = NULL, *files = NULL;
	int val, err, start = 1;

	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(cmd);

	while ((p = *++argv) != NULL) {
		if (!strcmp(p, "--help") || !strcmp(p, "-h")) {
			printf("%s", cmd->help);
			return BCME_OK;
		}

		if (!strncmp(p, "-", 1)) {
			opt = p[1];
			if (strlen(p) > 2) {
				fprintf(stderr, "wl_msch_dump: only single char options, "
					"error on param \"%s\"\n", p);
				return BCME_BADARG;
			}

			argv++;
			if (*argv == NULL) {
				fprintf(stderr, "wl_msch_dump: missing value parameter "
					"after \"%s\"\n", p);
				return BCME_USAGE_ERROR;
			}

			if (opt == 'D')
				dir = *argv;
			else if (opt == 'F')
				files = *argv;
			else {
				fprintf(stderr, "error param: %s\n", p);
				return BCME_BADARG;
			}
		} else if (!fname) {
			fname = p;
		} else {
			fprintf(stderr, "error param: %s\n", p);
			return BCME_BADARG;
		}
	}

	if ((err = wlu_iovar_getint(wl, "msch_collect", &val)) < 0)
		return err;

	if (!(val & WL_MSCH_CMD_ENABLE_BIT))
		return BCME_NOTREADY;

	if (fname) {
		if (!(mschfp = fopen(fname, "wb"))) {
			perror(fname);
			fprintf(stderr, "Cannot open file %s\n", fname);
			return BCME_BADARG;
		}
	}

	wl_init_frm_array(dir, files);

	err = wlu_var_getbuf(wl, "msch_dump", &start, sizeof(int), &ptr);
	while (err >= 0) {
		msch_collect_tlv_t *ptlv = (msch_collect_tlv_t *)ptr;

		type = dtoh16(ptlv->type);
		data = ptlv->value;

		wl_msch_dump_data(data, type);

		err = wlu_var_getbuf(wl, "msch_dump", NULL, 0, &ptr);
	}

	if (mschfp) {
		fflush(mschfp);
		fclose(mschfp);
		mschfp = NULL;
	}

	fflush(stdout);
	return BCME_OK;
}

#if defined(linux)

int wl_format_ssid(char* ssid_buf, uint8* ssid, int ssid_len);

int wl_msch_event_check(void *wl, cmd_t *cmd, char **argv)
{
	int fd, err, octets;
	struct sockaddr_ll sll;
	struct ifreq ifr;
	char ifnames[IFNAMSIZ] = {"eth0"};
	bcm_event_t *event;
	char *data;
	int event_type, reason;
	uint8 event_inds_mask[WL_EVENTING_MASK_LEN];
	char *p, *fname = NULL, *dir = NULL, *files = NULL;
	int opt, val;

	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(cmd);

	if (argv[1] == NULL) {
		fprintf(stderr, "<ifname> param is missing\n");
		return -1;
	}

	if (*++argv) {
		if (!strcmp(*argv, "--help") || !strcmp(*argv, "-h")) {
			printf("%s", cmd->help);
			return BCME_OK;
		}
		strncpy(ifnames, *argv, (IFNAMSIZ - 1));
	}

	if ((err = wlu_iovar_getint(wl, "msch_event", &val)) < 0)
		return err;

	bzero(&ifr, sizeof(ifr));
	strncpy(ifr.ifr_name, ifnames, (IFNAMSIZ - 1));

	fd = socket(PF_PACKET, SOCK_RAW, hton16(ETHER_TYPE_BRCM));
	if (fd < 0) {
		fprintf(stderr, "Cannot create socket %d\n", fd);
		return -1;
	}

	err = ioctl(fd, SIOCGIFINDEX, &ifr);
	if (err < 0) {
		fprintf(stderr, "Cannot get iface:%s index \n", ifr.ifr_name);
		goto exit;
	}

	bzero(&sll, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = hton16(ETHER_TYPE_BRCM);
	sll.sll_ifindex = ifr.ifr_ifindex;
	err = bind(fd, (struct sockaddr *)&sll, sizeof(sll));
	if (err < 0) {
		fprintf(stderr, "Cannot bind %d\n", err);
		goto exit;
	}

	while (*++argv) {
		opt = 0;
		p = *argv;
		if (p[0] == '+') {
			opt = 1;
			p++;
		}
		else if (p[0] == '-') {
			opt = 2;
			p++;
		}

		if (opt == 0) {
			fname = p;
			if (mschfp == NULL) {
				if (!(mschfp = fopen(fname, "wb"))) {
					perror(fname);
					fprintf(stderr, "Cannot open file %s\n", fname);
					err = BCME_BADARG;
					goto exit;
				}
			}
			else {
				err = BCME_BADARG;
				fprintf(stderr, "error param: %s\n", p);
				goto exit;
			}
		} else {
			if (opt == 2 && (!strcmp(p, "all") || !strcmp(p, "a"))) {
				val &= ~WL_MSCH_CMD_ALL_BITS;
			} else if (!strcmp(p, "profiler") || !strcmp(p, "p")) {
				if (opt == 1)
					val |= WL_MSCH_CMD_PROFILE_BIT;
				else
					val &= ~WL_MSCH_CMD_PROFILE_BIT;
			}
			else if (!strcmp(p, "callback") || !strcmp(p, "c")) {
				if (opt == 1)
					val |= WL_MSCH_CMD_CALLBACK_BIT;
				else
					val &= ~WL_MSCH_CMD_CALLBACK_BIT;
			}
			else if (!strcmp(p, "register") || !strcmp(p, "r")) {
				if (opt == 1)
					val |= WL_MSCH_CMD_REGISTER_BIT;
				else
					val &= ~WL_MSCH_CMD_REGISTER_BIT;
			}
			else if (!strcmp(p, "error") || !strcmp(p, "e")) {
				if (opt == 1)
					val |= WL_MSCH_CMD_ERROR_BIT;
				else
					val &= ~WL_MSCH_CMD_ERROR_BIT;
			}
			else if (!strcmp(p, "debug") || !strcmp(p, "d")) {
				if (opt == 1)
					val |= WL_MSCH_CMD_DEBUG_BIT;
				else
					val &= ~WL_MSCH_CMD_DEBUG_BIT;
			}
			else if (!strcmp(p, "info") || !strcmp(p, "i")) {
				if (opt == 1)
					val |= WL_MSCH_CMD_INFOM_BIT;
				else
					val &= ~WL_MSCH_CMD_INFOM_BIT;
			}
			else if (!strcmp(p, "trace") || !strcmp(p, "t")) {
				if (opt == 1)
					val |= WL_MSCH_CMD_TRACE_BIT;
				else
					val &= ~WL_MSCH_CMD_TRACE_BIT;
			}
			else if (opt == 2 && !strcmp(p, "D") && *++argv) {
				dir = *argv;
				printf("dir: %s\n", dir);
			}
			else if (opt == 2 && !strcmp(p, "F") && *++argv) {
				files = *argv;
				printf("files: %s\n", files);
			}
			else {
				err = BCME_EPERM;
				goto exit;
			}
		}
	}

	data = &mschdata[sizeof(bcm_event_t)];

	/*  read current mask state  */
	if ((err = wlu_iovar_get(wl, "event_msgs", &event_inds_mask, WL_EVENTING_MASK_LEN))) {
		fprintf(stderr, "couldn't read event_msgs\n");
		goto exit;
	}
	event_inds_mask[WLC_E_MSCH / 8] |= (1 << (WLC_E_MSCH % 8));
	if ((err = wlu_iovar_set(wl, "event_msgs", &event_inds_mask, WL_EVENTING_MASK_LEN))) {
		fprintf(stderr, "couldn't write event_msgs\n");
		goto exit;
	}

	val &= ~WL_MSCH_CMD_VER_MASK;
	val |= (WL_MSCH_CMD_ENABLE_BIT | (WL_MSCH_PROFILER_VER << WL_MSCH_CMD_VER_SHIFT));
	if ((err = wlu_iovar_setint(wl, "msch_event", val))) {
		fprintf(stderr, "couldn't start msch event\n");
		goto exit;
	}

	wl_init_frm_array(dir, files);

	printf("wating for MSCH events :%s\n", ifr.ifr_name);

	while (1) {
		fflush(stdout);
		octets = recv(fd, mschdata, WL_MSCH_PROFILER_BUFFER_SIZE, 0);

		if (octets <= 0)  {
			/* sigterm */
			err = -1;
			break;
		}

		event = (bcm_event_t *)mschdata;
		event_type = ntoh32(event->event.event_type);
		reason = ntoh32(event->event.reason);

		if ((event_type != WLC_E_MSCH)) {
			if (event_type == WLC_E_ESCAN_RESULT) {
				wl_escan_result_v2_t* escan_data =
					(wl_escan_result_v2_t*)data;
				uint16	i;
				MSCH_EVENTS_PRINTF1("MACEVENT_%d: WLC_E_ESCAN_RESULT:", event_type);
				for (i = 0; i < escan_data->bss_count; i++) {
					wl_bss_info_v109_1_t *bi = (wl_bss_info_v109_1_t*)
						&escan_data->bss_info[i];
					char ssidbuf[SSID_FMT_BUF_LEN];
					char chspec_str[CHANSPEC_STR_LEN];
					wl_format_ssid(ssidbuf, bi->SSID, bi->SSID_len);
					MSCH_EVENTS_PRINTF2(" SSID: \"%s\", Channel: %s;",
						ssidbuf, wf_chspec_ntoa(bi->chanspec, chspec_str));
				}
				MSCH_EVENTS_PRINTF("\n");
			} else {
				char *event_name;
				switch (event_type) {
				case WLC_E_JOIN:
					event_name = "WLC_E_JOIN";
					break;

				case WLC_E_AUTH:
					event_name = "WLC_E_AUTH";
					break;

				case WLC_E_ASSOC:
					event_name = "WLC_E_ASSOC";
					break;

				case WLC_E_LINK:
					event_name = "WLC_E_LINK";
					break;

				case WLC_E_ROAM:
					event_name = "WLC_E_ROAM";
					break;

				case WLC_E_SCAN_COMPLETE:
					event_name = "WLC_E_SCAN_COMPLETE";
					break;

				case WLC_E_SCAN_CONFIRM_IND:
					event_name = "WLC_E_SCAN_CONFIRM_IND";
					break;

				case WLC_E_ASSOC_REQ_IE:
					event_name = "WLC_E_ASSOC_REQ_IE";
					break;

				case WLC_E_ASSOC_RESP_IE:
					event_name = "WLC_E_ASSOC_RESP_IE";
					break;

				case WLC_E_BSSID:
					event_name = "WLC_E_BSSID";
					break;

				default:
					event_name = "Unknown Event";
				}
				MSCH_EVENTS_PRINTF2("MACEVENT_%d: %s\n", event_type, event_name);
			}
			continue;
		}

		wl_msch_dump_data(data, reason);

		if ((reason & WL_MSCH_PROFILER_TYPE_MASK) == WL_MSCH_PROFILER_EXIT)
			goto exit;
	}
exit:
	/* if we ever reach here */
	close(fd);
	if (mschfp) {
		fflush(mschfp);
		fclose(mschfp);
		mschfp = NULL;
	}

	/* Read the event mask from driver and mask the event WLC_E_MSCH */
	if (!(err = wlu_iovar_get(wl, "event_msgs", &event_inds_mask, WL_EVENTING_MASK_LEN))) {
		event_inds_mask[WLC_E_MSCH / 8] &= (~(1 << (WLC_E_MSCH % 8)));
		err = wlu_iovar_set(wl, "event_msgs", &event_inds_mask, WL_EVENTING_MASK_LEN);
	}

	fflush(stdout);
	return (err);
}
#endif /* linux */

typedef struct wl_msch_profiler_struct {
	uint32	start_ptr;
	uint32	write_ptr;
	uint32	write_size;
	uint32	read_ptr;
	uint32	read_size;
	uint32	total_size;
	uint32  buffer;
} wl_msch_profiler_struct_t;

#define MSCH_MAGIC_1		0x4d534348
#define MSCH_MAGIC_2		0x61676963

#undef ROUNDUP
#define	ROUNDUP(x)		(((x) + 3) & (~0x03))

#define MAX_PROFILE_DATA_SIZE	4096

int wl_msch_profiler(void *wl, cmd_t *cmd, char **argv)
{
	char *fname_r = NULL, *fname_w = NULL, *dir = NULL, *files = NULL;
	char *buffer = NULL, *p, opt;
	FILE *fp = NULL;
	int err = BCME_OK;
	uint32 magicdata;
	uint32 rptr, rsize, wsize, tsize;
	wl_msch_profiler_struct_t profiler;
	bool found = FALSE;

	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(cmd);

	while ((p = *++argv) != NULL) {
		if (!strcmp(p, "--help") || !strcmp(p, "-h")) {
			printf("%s", cmd->help);
			return BCME_OK;
		}

		if (!strncmp(p, "-", 1)) {
			opt = p[1];
			if (strlen(p) > 2) {
				fprintf(stderr, "wl_msch_profiler: only single char options, "
					"error on param \"%s\"\n", p);
				return BCME_BADARG;
			}

			argv++;
			if (*argv == NULL) {
				fprintf(stderr, "wl_msch_profiler: missing value parameter "
					"after \"%s\"\n", p);
				return BCME_USAGE_ERROR;
			}

			if (opt == 'D')
				dir = *argv;
			else if (opt == 'F')
				files = *argv;
			else {
				fprintf(stderr, "error param: %s\n", p);
				return BCME_BADARG;
			}
		} else if (!fname_r) {
			fname_r = p;
		} else if (!fname_w) {
			fname_w = p;
		} else {
			fprintf(stderr, "error param: %s\n", p);
			return BCME_BADARG;
		}
	}

	if (fname_r == NULL) {
		fprintf(stderr, "<input filename> param is missing\n");
		return -1;
	}

	if (!(fp = fopen(fname_r, "rb"))) {
		perror(fname_r);
		fprintf(stderr, "Cannot open input file %s\n", fname_r);
		err = BCME_BADARG;
		goto exit;
	}

	rptr = 0;
	while ((rsize = fread(&magicdata, 1, sizeof(uint32), fp)) > 0) {
		rptr += rsize;
		magicdata = dtoh32(magicdata);
		if (magicdata != MSCH_MAGIC_1)
			continue;

		if ((rsize = fread(&magicdata, 1, sizeof(uint32), fp)) > 0) {
			rptr += rsize;
			magicdata = dtoh32(magicdata);
			if (magicdata != MSCH_MAGIC_2)
				continue;
		}

		rsize = fread(&profiler, 1, sizeof(wl_msch_profiler_struct_t), fp);
		rptr += rsize;
		magicdata = dtoh32(profiler.buffer);

		if (((rptr ^ magicdata) & 0xffff) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		fprintf(stderr, "Cannot find profiler data from file %s\n", fname_r);
		err = BCME_NOTFOUND;
		goto exit;
	}

	if (fname_w) {
		if (!(mschfp = fopen(fname_w, "wb"))) {
			perror(fname_w);
			fprintf(stderr, "Cannot open file %s\n", fname_w);
			err = BCME_BADARG;
			goto exit;
		}
	}

	tsize = dtoh32(profiler.total_size);
	buffer = (char*)malloc(tsize + MAX_PROFILE_DATA_SIZE);
	if (buffer == NULL) {
		fprintf(stderr, "Cannot not allocate %d bytes for profiler buffer\n",
			tsize);
		err = BCME_NOMEM;
		goto exit;
	}

	if ((rsize = fread(buffer, 1, tsize, fp)) != tsize) {
		fprintf(stderr, "Cannot read profiler data from file %s, req %d, read %d\n",
			fname_r, tsize, rsize);
		err = BCME_BADARG;
		goto exit;
	}

	wsize = dtoh32(profiler.write_size);
	rptr = dtoh32(profiler.start_ptr);
	rsize = 0;

	wl_init_frm_array(dir, files);

	while (rsize < wsize) {
		msch_collect_tlv_t *ptlv = (msch_collect_tlv_t *)(buffer + rptr);
		int type, size, remain;
		char *data;

		size = ROUNDUP(WL_MSCH_PROFILE_HEAD_SIZE + dtoh16(ptlv->size));

		rsize += size;
		if (rsize > wsize)
			break;

		remain = tsize - rptr;
		if (remain >= size) {
			rptr += size;
			if (rptr == tsize)
				rptr = 0;
		} else {
			remain = size - remain;
			memcpy(buffer + tsize, buffer, remain);
			rptr = remain;
		}

		type = dtoh16(ptlv->type);
		data = ptlv->value;

		wl_msch_dump_data(data, type);
	}

	err = BCME_OK;

exit:
	if (fp)
		fclose(fp);

	if (buffer)
		free(buffer);

	if (mschfp) {
		fflush(mschfp);
		fclose(mschfp);
		mschfp = NULL;
	}

	fflush(stdout);
	return err;
}
