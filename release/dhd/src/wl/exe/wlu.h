/*
 * Common code for wl command line utility
 *
 * Copyright 2002, Broadcom Corporation
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Broadcom Corporation.
 *
 * $Id: wlu.h 472405 2014-04-23 23:46:55Z $
 */

#ifndef _wlu_h_
#define _wlu_h_

#include <proto/bcmipv6.h>
#include "wlu_cmd.h"

typedef struct {
	uint value;
	const char *string;
} dbg_msg_t;

extern const char *wlu_av0;
extern int g_wlc_idx;

/* parse common option */
extern int wl_option(char ***pargv, char **pifname, int *phelp);
extern void wl_cmd_init(void);
extern void wlu_init(void);

/* print usage */
extern void wl_cmd_usage(FILE *fid, cmd_t *cmd);
extern void wl_usage(FILE *fid, cmd_t *port_cmds);
extern void wl_cmds_usage(FILE *fid, cmd_t *port_cmds);

/* print helpers */
extern void wl_printlasterror(void *wl);
extern void wl_printint(int val);

/* pretty print an SSID */
extern int wl_format_ssid(char* buf, uint8* ssid, int ssid_len);

/* pretty hex print a contiguous buffer */
extern void wl_hexdump(uchar *buf, uint nbytes);

/* check driver version */
extern int wl_check(void *wl);

extern int wlu_bssiovar_setbuf(void* wl, const char *iovar, int bssidx,
	void *param, int paramlen, void *bufptr, int buflen);
extern int wlu_var_getbuf(void *wl, const char *iovar,
	void *param, int param_len, void **bufptr);
extern int wlu_var_getbuf_sm(void *wl, const char *iovar,
	void *param, int param_len, void **bufptr);
extern int wlu_var_getbuf_med(void *wl, const char *iovar,
	void *param, int param_len, void **bufptr);

extern cmd_func_t wl_void;
extern cmd_func_t wl_var_void;
extern cmd_func_t wl_var_setint;
extern cmd_func_t wl_reg;
extern cmd_func_t wl_phy_rate;
extern cmd_func_t wl_ssid;
extern cmd_func_t wl_iov_mac;
extern cmd_func_t wl_sta_info;

extern int ARGCNT(char **argv);

/* return the address of bufstruct_wlu, global buf */
extern char *wl_get_buf(void);

extern int wl_cfg_option(char **argv, const char *fn_name, int *bsscfg_idx, int *consumed);
extern int wl_scan_prep(void *wl, cmd_t *cmd, char **argv,
	wl_scan_params_t *params, int *params_size);
extern chanspec_t wl_chspec_to_driver(chanspec_t chanspec);
extern chanspec_t wl_chspec_from_driver(chanspec_t chanspec);
extern int wl_parse_channel_list(char* list_str, uint16* channel_list, int channel_num);


/* Format a ratespec for output of any of the wl_rate() iovars */
extern char* wl_rate_print(char *rate_buf, uint32 rspec);
/* convert rate string in Mbit/s format, like "11", "5.5", to internal 500 Kbit/s units */
extern int rate_string2int(char *s);
/* convert rate internal 500 Kbits/s units to string in Mbits/s format, like "11", "5.5" */
extern char* rate_int2string(char *rate_buf, int val);

extern int hexstr2hex(char *str);
extern char* find_pattern(char **argv, const char *pattern, uint *val);
extern char* find_pattern2(char **argv, const char *pattern, uint *val, int vnum);
extern int wl_atoipv6(const char *a, struct ipv6_addr *n);
extern char * wl_ipv6toa(const void *ipv6);
extern int wlu_reg3args(void *wl, cmd_t *cmd, char **argv);

/* Convert user's input in hex pattern to byte-size mask */
extern int wl_pattern_atoh(char *src, char *dst);

/* register commands for a module */
extern void wl_module_cmds_register(cmd_t *cmds);

extern cmd_t *wlu_find_cmd(char *name);

/* wluc_module init functions */
extern void wluc_phy_module_init(void);
extern void wluc_wnm_module_init(void);
extern void wluc_cac_module_init(void);
extern void wluc_rmc_module_init(void);
extern void wluc_rrm_module_init(void);
extern void wluc_pfn_module_init(void);
extern void wluc_p2p_module_init(void);
extern void wluc_tdls_module_init(void);
extern void wluc_trf_mgmt_module_init(void);
extern void wluc_proxd_module_init(void);
extern void wluc_p2po_module_init(void);
extern void wluc_anqpo_module_init(void);

/* wl functions used by the ndis wl. */
extern void dump_rateset(uint8 *rates, uint count);
extern uint freq2channel(uint freq);
extern int wl_ether_atoe(const char *a, struct ether_addr *n);
extern char *wl_ether_etoa(const struct ether_addr *n);
struct ipv4_addr;	/* forward declaration */
extern int wl_atoip(const char *a, struct ipv4_addr *n);
extern char *wl_iptoa(const struct ipv4_addr *n);
extern cmd_func_t wl_int;
extern cmd_func_t wl_varint;
extern void wl_dump_raw_ie(bcm_tlv_t *ie, uint len);
extern int wl_mk_ie_setbuf(const char *command, uint32 pktflag_ok, char **argv,
	vndr_ie_setbuf_t **buf, int *buf_len);
extern cmd_func_t wl_list_ie;

extern void wl_printlasterror(void *wl);
extern bool wc_cmd_check(const char *cmd);


/* functions for downloading firmware to a device via serial or other transport */
#ifdef SERDOWNLOAD
extern int dhd_init(void *dhd, cmd_t *cmd, char **argv);
extern int dhd_download(void *dhd, cmd_t *cmd, char **argv);
#endif /* SERDOWNLOAD */

#ifdef BCMDLL
#ifdef LOCAL
extern FILE *dll_fd;
#else
extern void * dll_fd_out;
extern void * dll_fd_in;
#endif
#undef printf
#undef fprintf
#define printf printf_to_fprintf	/* printf to stdout */
#define fprintf fprintf_to_fprintf	/* fprintf to stderr */
extern void fprintf_to_fprintf(FILE * stderror, const char *fmt, ...);
extern void printf_to_fprintf(const char *fmt, ...);
extern void raw_puts(const char *buf, void *dll_fd_out);
#define	fputs(buf, stdout) raw_puts(buf, dll_fd_out)
#endif /* BCMDLL */

#define	PRNL()		pbuf += sprintf(pbuf, "\n")

#define RAM_SIZE_4325  0x60000
#define RAM_SIZE_4329  0x48000
#define RAM_SIZE_43291 0x60000
#define RAM_SIZE_4330_a1  0x3c000
#define RAM_SIZE_4330_b0  0x48000

/* useful macros */
#ifndef ARRAYSIZE
#define ARRAYSIZE(a)  (sizeof(a)/sizeof(a[0]))
#endif /* ARRAYSIZE */

/* buffer length needed for wl_format_ssid
 * 32 SSID chars, max of 4 chars for each SSID char "\xFF", plus NULL
 */
#ifndef SSID_FMT_BUF_LEN
#define SSID_FMT_BUF_LEN (4*32+1)	/* Length for SSID format string */
#endif

#define USAGE_ERROR  -1		/* Error code for Usage */
#define CMD_DEPRECATED -4 /* Commands that are functionally deprecated or don't provide
			   * a useful value to a specific OS port of wl
			   */

/* integer output format */
#define INT_FMT_DEC	0	/* signed integer */
#define INT_FMT_UINT	1	/* unsigned integer */
#define INT_FMT_HEX	2	/* hexdecimal */

#define DIV_QUO(num, div) ((num)/div)  /* Return the quotient of division to avoid floats */
#define DIV_REM(num, div) (((num%div) * 100)/div) /* Return the remainder of division */

/* command line argument usage */
#define CMD_ERR	-1	/* Error for command */
#define CMD_OPT	0	/* a command line option */
#define CMD_WL	1	/* the start of a wl command */

#define SCAN_USAGE	"" \
"\tDefault to an active scan across all channels for any SSID.\n" \
"\tOptional arg: SSIDs, list of [up to 10] SSIDs to scan (comma or space separated).\n" \
"\tOptions:\n" \
"\t-s S, --ssid=S\t\tSSIDs to scan\n" \
"\t-t ST, --scan_type=ST\t[active|passive|prohibit|offchan|hotspot] scan type\n" \
"\t--bss_type=BT\t\t[bss/infra|ibss/adhoc] bss type to scan\n" \
"\t-b MAC, --bssid=MAC\tparticular BSSID MAC address to scan, xx:xx:xx:xx:xx:xx\n" \
"\t-n N, --nprobes=N\tnumber of probes per scanned channel\n" \
"\t-a N, --active=N\tdwell time per channel for active scanning\n" \
"\t-p N, --passive=N\tdwell time per channel for passive scanning\n" \
"\t-h N, --home=N\t\tdwell time for the home channel between channel scans\n" \
"\t-c L, --channels=L\tcomma or space separated list of channels to scan" \

#endif /* _wlu_h_ */
