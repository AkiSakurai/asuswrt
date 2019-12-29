/**
 * @file
 * @brief
 * Named dump callback registration for WLC (excluding BMAC)
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
 * $Id: wlc_dump.c 775546 2019-06-04 02:40:32Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_bmac.h>
#include <wlc_dump_reg.h>
#include <wlc_dump.h>

#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(BCMDBG_PHYDUMP) || \
	defined(WLTEST) || defined(TDLS_TESTBED) || defined(BCMDBG_AMPDU) || \
	defined(MCHAN_MINIDUMP) || defined(BCM_DNGDMP) || defined(DNG_DBGDUMP) || \
	defined(ULP_DUMP) || defined(BCMDBG_RSDB)
#define WLC_DUMP_FULL_SUPPORT
#endif // endif

/* registry capacity */
#ifndef WLC_DUMP_NUM_REGS
#ifdef WLC_DUMP_FULL_SUPPORT
#define WLC_DUMP_NUM_REGS 128
#elif defined(DL_RU_STATS_DUMP) || defined(UL_RU_STATS_DUMP) || defined(DUMP_MUTX) || \
	defined(DUMP_MURX) || defined(DUMP_D11CNTS) || defined(DUMP_RATELINKMEM)
#define WLC_DUMP_NUM_REGS 11 /* TODO: merge all these defines to a common release dump define ? */
#else
#define WLC_DUMP_NUM_REGS 4
#endif // endif
#endif /* WLC_DUMP_NUM_REGS */

/* iovar table */
enum {
	IOV_DUMP = 1,		/* Dump iovar */
	IOV_DUMP_CLR = 2,
	IOV_LAST
};

static const bcm_iovar_t dump_iovars[] = {
	{"dump", IOV_DUMP, IOVF_OPEN_ALLOW, 0, IOVT_BUFFER, 0},
	{"dump_clear", IOV_DUMP_CLR, IOVF_OPEN_ALLOW, 0, IOVT_BUFFER, 0},
	{NULL, 0, 0, 0, 0, 0}
};

/* module info */
struct wlc_dump_info {
	wlc_info_t *wlc;
	wlc_dump_reg_info_t *reg;
};

static int wlc_dump_clr(wlc_dump_info_t *, char *, uint, char *, uint);
static int wlc_dump(wlc_dump_info_t *, char *, uint, char *, uint);

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/** Invoke the given named dump callback */
static int
wlc_dump_op(wlc_dump_info_t *dumpi, const char *name,
	struct bcmstrbuf *b)
{
	wlc_info_t *wlc = dumpi->wlc;
	int ret = BCME_NOTFOUND;

	if (dumpi->reg != NULL) {
		ret = wlc_dump_reg_invoke_dump_fn(dumpi->reg, name, b);
	}
	if (ret == BCME_NOTFOUND) {
		ret = wlc_bmac_dump(wlc->hw, name, b);
	}

	return ret;
}

/** Invoke the given named dump clear callback */
static int
wlc_dump_clr_op(wlc_dump_info_t *dumpi, const char *name)
{
	wlc_info_t *wlc = dumpi->wlc;
	int ret = BCME_NOTFOUND;

	if (dumpi->reg != NULL) {
		ret = wlc_dump_reg_invoke_clr_fn(dumpi->reg, name);
	}
	if (ret == BCME_NOTFOUND) {
		ret = wlc_bmac_dump_clr(wlc->hw, name);
	}

	return ret;
}

/* Tokenize the buffer (delimiter is ' ' or '\0') and return the null terminated string
 * in param 'name' if the string fits in 'name'. The string will be truncated in 'name'
 * if it's longer than 'name/name_len', in which case BCME_BADARG is returned in 'err'.
 * Return the name string length in 'buf' as the function return value.
 */
static int
wlc_dump_next_name(char *name, int name_len, char **buf, uint *buf_len, int *err)
{
	char *p = *buf;
	int l = *buf_len;
	int i = 0;

	/* skip leading space in 'buf' */
	while (l > 0 &&
	       bcm_isspace(*p) && *p != '\0') {
		p++;
		l--;
	}

	/* find the end of the name string while copying to 'name'.
	 * name string is terminated by space or null in 'buf'.
	 * the copied string may not be null terminated.
	 */
	while (l > 0 &&
	       !bcm_isspace(*p) && *p != '\0') {
		if (i < name_len) {
			name[i] = *p;
		}
		i++;
		p++;
		l--;
	}

	/* terminate the name if the string plus the null terminator fits 'name',
	 * or return error otherwise.
	 */
	if (i >= name_len) {
		*err = BCME_BUFTOOSHORT;
	}
	else {
		*err = BCME_OK;
		name[i] = 0;
	}

	/* update the buffer pointer and length */
	*buf = p;
	*buf_len = l;

	/* return the name string length in 'buf' */
	return i;
}

/* all arguments must be inside []. */
static int
wlc_dump_next_args(char *args, int args_len, char **buf, uint *buf_len, int *err)
{
	char *p = *buf;
	char *start;
	int l = *buf_len;
	int i = 0;

	/* skip leading space */
	while (l > 0 &&
	       bcm_isspace(*p) && *p != '\0') {
		p++;
		l--;
	}

	if (*p == '[') {
	         /* skip optional [ and leading spaces */
	        p++;
		l--;
		/* skip leading space after first '[' */
		while (l > 0 &&
		       bcm_isspace(*p) && *p != '\0') {
	                p++;
			l--;
		}
		if (*p == '\0') {
		        return 0;
		}
	} else if (*p != '-') {
	        /* If no '[' then first arg must begin with '-' */
	        return 0;
	}

	start = p;

	/* find ']' */
	while (l > 0 &&
		*p != '\0' && *p != ']') {
		if (i < args_len) {
			args[i] = *p;
		}
		i++;
		p++;
		l--;
	}

	/* If we found ']' advance */
	if (*p != '\0' && start != p) {
	        p++;
	        l--;
	}

	/* terminate the args if the string plus the null terminator fits 'args',
	 * or return error otherwise.
	 */
	if (i >= args_len) {
		*err = BCME_BUFTOOSHORT;
	}
	else {
		*err = BCME_OK;
		args[i] = 0;
	}

	/* update the buffer pointer and length */
	*buf = p;
	*buf_len = l;

	/* return the name string length in 'buf' */
	return i;
}

/* including null terminator */
#define DUMP_NAME_BUF_LEN 17
#define DUMP_ARGUS_BUF_LEN 65

/* dump by name (could be a list of names separated by space)
 * in in_buf.
 */
static int
wlc_dump(wlc_dump_info_t *dumpi, char *in_buf, uint in_len, char *out_buf, uint out_len)
{
	wlc_info_t *wlc = dumpi->wlc;
	int err = BCME_NOTFOUND, aerr = BCME_NOTFOUND;
	int name_len, args_len = 0;
	struct bcmstrbuf b;
	char name[DUMP_NAME_BUF_LEN];
	char args[DUMP_ARGUS_BUF_LEN];
	bool sep = FALSE;
	char *buf = NULL;
	uint buf_len = 0;

	/* read the first name */
	name_len = wlc_dump_next_name(name, sizeof(name), &in_buf, &in_len, &err);
	if (name_len > 0) {
		args_len = wlc_dump_next_args(args, sizeof(args),
			&in_buf, &in_len, &aerr);
		if (args_len > 0)
			wlc->dump_args = args;
		else
			wlc->dump_args = NULL;
	}

	/* bcm_binit() will NUL terminate out_buf.
	 * Since the out_buf and in_buf pointers may be the same, it is only safe to
	 * init the bcmstrbuf after we have read past the first characters with the
	 * call to wlc_dump_next_name() above.
	 */
	bcm_binit(&b, out_buf, out_len);

	/* do default action if no dump name is present... */
	if (name_len == 0) {
		WL_PRINT(("doing default dump...\n"));
#if defined(WLTEST) && defined(WLTINYDUMP)
		err = wlc_dump_op(dumpi, "tiny", &b);
#elif defined(BCMDBG) || defined(BCMDBG_DUMP)
		err = wlc_dump_op(dumpi, "default", &b);
#endif /* defined(WLTEST) && defined(WLTINYDUMP) */
		goto exit;
	}

	/* dump all names in the name list...unless there's error */
	do {
		char next[DUMP_NAME_BUF_LEN];
		char next_args[DUMP_ARGUS_BUF_LEN];
		bool more;
		int nerr, naerr;

		/* we could ignore the name that is too long instead */
		if (err == BCME_BUFTOOSHORT) {
			name[sizeof(name)- 1] = 0;
			WL_ERROR(("%s: name %s... is too long\n",
			          __FUNCTION__, name));
			err = BCME_BADARG;
			goto exit;
		}
		if (aerr == BCME_BUFTOOSHORT) {
			args[sizeof(args)- 1] = 0;
			WL_ERROR(("%s: args %s... is too long\n",
			          __FUNCTION__, args));
			err = BCME_BADARG;
			goto exit;
		}

		/* look ahead, do we have more dump ... */
		more = wlc_dump_next_name(next, sizeof(next), &in_buf, &in_len, &nerr);
		args_len = 0;
		if (more > 0) {
			args_len = wlc_dump_next_args(next_args, sizeof(next_args),
				&in_buf, &in_len, &naerr);

			/* also allocate a buffer to copy the remaining of names over
			 * as the in_buf and out_buf may be the same and out_buf will
			 * be overwritten next.
			 */
			if (nerr == BCME_OK &&
			    buf == NULL &&
			    (buf_len = (uint)strnlen(in_buf, in_len)) > 0) {
				if ((buf = MALLOC(dumpi->wlc->osh, buf_len)) == NULL) {
					err = BCME_NOMEM;
					goto exit;
				}
				memcpy(buf, in_buf, buf_len);
				in_buf = buf;
				in_len = buf_len;
			}
		}

		/* only print the separator if there are more than one dump */
		sep |= more;
		if (sep) {
			bcm_bprintf(&b, "\n%s %s:------\n", name,
				wlc->dump_args ? wlc->dump_args : "");
		}

		if ((err = wlc_dump_op(dumpi, name, &b)) != BCME_OK) {
			break;
		}

		if (!more) {
			break;
		}

		ASSERT(sizeof(name) == sizeof(next));
		ASSERT(sizeof(args) == sizeof(next_args));
		memcpy(name, next, sizeof(next));
		if (args_len > 0) {
			wlc->dump_args = args;
			memcpy(args, next_args, sizeof(next_args));
		}
		else {
			wlc->dump_args = NULL;
		}

		err = nerr;
		aerr = naerr;
	} while (TRUE);

exit:
	if (buf != NULL) {
		MFREE(dumpi->wlc->osh, buf, buf_len);
	}

	wlc->dump_args = NULL;

	/* XXX UTF requests to replace BCME_NOTFOUND to BCME_UNSUPPORTED
	 * for easy error handling so do it here...
	 */
	if (err == BCME_NOTFOUND) {
		WL_INFORM(("wl%d: %s: forcing return code to BCME_UNSUPPORTED\n",
		           dumpi->wlc->pub->unit, __FUNCTION__));
		err = BCME_UNSUPPORTED;
	}

	return err;
}

/* Dump clear the given name (could be a list of names separated by space)
 * in in_buf.
 */
static int
wlc_dump_clr(wlc_dump_info_t *dumpi, char *in_buf, uint in_len, char *out_buf, uint out_len)
{
	char name[DUMP_NAME_BUF_LEN];
	int err = BCME_NOTFOUND;

	/* dump clear all names in the name list...unless there's error */
	while (wlc_dump_next_name(name, sizeof(name), &in_buf, &in_len, &err) > 0) {
		if (err == BCME_BUFTOOSHORT) {
			name[sizeof(name)- 1] = 0;
			WL_ERROR(("%s: name %s... is too long\n",
			          __FUNCTION__, name));
			err = BCME_BADARG;
			break;
		}
		if ((err = wlc_dump_clr_op(dumpi, name)) != BCME_OK) {
			break;
		}
	}

	/* XXX UTF requests to replace BCME_NOTFOUND to BCME_UNSUPPORTED
	 * for easy error handling so do it here...
	 */
	if (err == BCME_NOTFOUND) {
		WL_INFORM(("wl%d: %s: forcing return code to BCME_UNSUPPORTED\n",
		           dumpi->wlc->pub->unit, __FUNCTION__));
		err = BCME_UNSUPPORTED;
	}

	return err;
}

/* 'wl dump [default]' command */
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static char *const def_dump_list[] = {
	"wlc",
	"phystate",
	"bsscfg",
	"bssinfo",
	"ratestuff",
	"stats",
	"pio",
	"dma",
	"wme",
	"ampdu",
	"wet",
	"toe",
	"led",
	"amsdu",
	"cac",
	"trfmgmt_stats",
	"trfmgmt_shaping"
};

/** Format a general info dump */
static int
wlc_dump_default(void *ctx, struct bcmstrbuf *b)
{
	wlc_dump_info_t *dumpi = ctx;
	uint i;
	int err;

	for (i = 0; i < ARRAYSIZE(def_dump_list); i ++) {
		bcm_bprintf(b, "\n%s:------\n", def_dump_list[i]);
		err = wlc_dump_op(dumpi, def_dump_list[i], b);
		if (err != BCME_OK) {
			bcm_bprintf(b, "\n%s: err %d\n", def_dump_list[i], err);
		}
	}

	return BCME_OK;
}

/* dump the dump registry internals */
static int
wlc_dump_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_dump_info_t *dumpi = ctx;
	wlc_info_t *wlc = dumpi->wlc;

	if (dumpi->reg != NULL) {
		wlc_dump_reg_dump(dumpi->reg, b);
	}
	wlc_bmac_dump_dump(wlc->hw, b);

	return BCME_OK;
}
#endif /* BCMDBG || BCMDBG_DUMP */

#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(WLTEST)
/** This function lists all dump names */
static int
wlc_dump_list(void *arg, struct bcmstrbuf *b)
{
	wlc_dump_info_t *dumpi = arg;
	wlc_info_t *wlc = dumpi->wlc;

	bcm_bprintf(b, "\nRegistered dumps:\n");
	if (dumpi->reg != NULL) {
		wlc_dump_reg_list(dumpi->reg, b);
	}
	wlc_bmac_dump_list(wlc->hw, b);

	return BCME_OK;
}
#endif /* BCMDBG || BCMDBG_DUMP || WLTEST */

/** Register dump name and handlers. Calling function must keep 'dump function' */
int
wlc_dump_add_fns(wlc_pub_t *pub, const char *name,
	dump_fn_t dump_fn, clr_fn_t clr_fn, void *ctx)
{
	wlc_info_t *wlc = (wlc_info_t *)pub->wlc;

	if (wlc->dumpi->reg != NULL) {
		return wlc_dump_reg_add_fns(wlc->dumpi->reg, name, dump_fn, clr_fn, ctx);
	}

	return BCME_UNSUPPORTED;
}

/* iovar dispatcher */
static int
wlc_dump_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_dump_info_t *dumpi = hdl;
	int err = BCME_OK;

	BCM_REFERENCE(dumpi);

	switch (actionid) {

	case IOV_GVAL(IOV_DUMP):
		err = wlc_dump(dumpi, params, p_len, arg, len);
		break;

	case IOV_SVAL(IOV_DUMP_CLR):
		err = wlc_dump_clr(dumpi, params, p_len, arg, len);
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

/* attach/detach to/from the system */
wlc_dump_info_t *
BCMATTACHFN(wlc_dump_pre_attach)(wlc_info_t *wlc)
{
	wlc_dump_info_t *dumpi;

	if ((dumpi = MALLOCZ(wlc->osh, sizeof(*dumpi))) == NULL) {
		goto fail;
	}
	dumpi->wlc = wlc;
#if WLC_DUMP_NUM_REGS > 0
	if ((dumpi->reg = wlc_dump_reg_create(wlc->osh, WLC_DUMP_NUM_REGS)) == NULL) {
		goto fail;
	}
#endif // endif
	return dumpi;

fail:
	MODULE_DETACH(dumpi, wlc_dump_post_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_dump_post_detach)(wlc_dump_info_t *dumpi)
{
	wlc_info_t *wlc;

	if (dumpi == NULL)
		return;

	wlc = dumpi->wlc;
#if WLC_DUMP_NUM_REGS > 0
	if (dumpi->reg != NULL) {
		wlc_dump_reg_destroy(dumpi->reg);
	}
#endif // endif
	MFREE(wlc->osh, dumpi, sizeof(*dumpi));
}

/* attach/detach to/from wlc module registry */
int
BCMATTACHFN(wlc_dump_attach)(wlc_dump_info_t *dumpi)
{
	wlc_info_t *wlc = dumpi->wlc;

	if (wlc_module_register(wlc->pub, dump_iovars, "dump",
			dumpi, wlc_dump_doiovar,
			NULL, NULL, NULL) != BCME_OK) {
		goto fail;
	}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	wlc_dump_register(wlc->pub, "default", wlc_dump_default, dumpi);
	wlc_dump_register(wlc->pub, "dump", wlc_dump_dump, dumpi);
#endif // endif
#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(WLTEST)
	wlc_dump_register(wlc->pub, "list", wlc_dump_list, dumpi);
#endif // endif

	return BCME_OK;

fail:
	MODULE_DETACH(dumpi, wlc_dump_detach);
	return BCME_ERROR;
}

void
BCMATTACHFN(wlc_dump_detach)(wlc_dump_info_t *dumpi)
{
	wlc_info_t *wlc;

	if (dumpi == NULL)
		return;

	wlc = dumpi->wlc;
	(void)wlc_module_unregister(wlc->pub, "dump", dumpi);
}

int
wlc_dump_local(wlc_dump_info_t *dumpi, char * name, int dump_len)
{
	char * dump_buf;
	int err = BCME_OK;
	int len;
	char * ptr = NULL;
	int i = 0;
	char tmp = '\0';

	dump_buf = (char *)MALLOC(dumpi->wlc->osh, dump_len);

	if (dump_buf == NULL) {
		err = BCME_NOMEM;
		WL_ERROR(("%s: malloc of %d bytes failed...\b", __FUNCTION__, dump_len));
		goto end;
	}

	/* NULL terminate the string */
	dump_buf[0] = '\0';

	err = wlc_dump(dumpi, name, strlen(name) + 1, dump_buf, dump_len);

	/* Irrespective of return code print the data populated by dump API */
	len = strlen(dump_buf);

	if (len >= dump_len) {
		WL_ERROR(("%s: dump len(%d) greater than input buffer size(%d)\n",
			__FUNCTION__, len, dump_len));
		err = BCME_ERROR;
	}

	/* Print each line in dump buffer */
	while ((i + 1) < len) {
		/* Mark start of the current print line */
		ptr = &dump_buf[i];

		/* traverse till '\n' or end of buffer */
		while ((i < len) && dump_buf[i] && dump_buf[i] != '\n') {
			i++;
		}

		/* If '\n' add '\0' after this to allow the printing current line */
		if (dump_buf[i]) {
			tmp = dump_buf[i + 1];
			dump_buf[i + 1] = '\0';
		}

#ifdef ENABLE_CORECAPTURE
		WIFICC_LOGDEBUG(("%s", ptr));
#else
		WL_PRINT(("%s", ptr));
#endif /* ENABLE_CORECAPTURE */

		/* Restore the character after '\n' */
		if (tmp) {
			dump_buf[i+1] = tmp;
			tmp = '\0';
		}

		i++;
	}
end:
	if (dump_buf) {
		MFREE(dumpi->wlc->osh, dump_buf, dump_len);
	}

	return err;
}
