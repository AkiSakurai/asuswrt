/*
 * Console support for rte.
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: hnd_cons.h 472405 2014-04-23 23:46:55Z $
 */
#ifndef	_hnd_cons_h_
#define	_hnd_cons_h_

#include <typedefs.h>
#include <siutils.h>

#if defined(RWL_DONGLE) || defined(UART_REFLECTOR)
/* For Dongle uart tranport max cmd len is 256 bytes + header length (16 bytes)
 *  In case of ASD commands we are not sure about how much is the command size
 *  To be on the safe side, input buf len CBUF_LEN is increased to max (512) bytes.
 */
#define RWL_MAX_DATA_LEN 	(512 + 8)	/* allow some extra bytes for '/n' termination */
#define CBUF_LEN	(RWL_MAX_DATA_LEN + 64)  /* allow 64 bytes for header ("rwl...") */
#else
#define CBUF_LEN	(128)
#endif /* RWL_DONGLE || UART_REFLECTOR */

#if defined(BCMDBG)
#define LOG_BUF_LEN	(16 * 1024)
#elif defined(ATE_BUILD)
#define LOG_BUF_LEN	(2 * 1024)
#else
#define LOG_BUF_LEN	1024
#endif

#ifdef BOOTLOADER_CONSOLE_OUTPUT
#undef RWL_MAX_DATA_LEN
#undef CBUF_LEN
#undef LOG_BUF_LEN
#define RWL_MAX_DATA_LEN (4 * 1024 + 8)
#define CBUF_LEN	(RWL_MAX_DATA_LEN + 64)
#define LOG_BUF_LEN (16 * 1024)
#endif

typedef struct {
#ifdef BCMDONGLEHOST
	uint32		buf;		/* Can't be pointer on (64-bit) hosts */
#else
	char		*buf;
#endif
	uint		buf_size;
	uint		idx;
	uint		out_idx;	/* output index */
} hnd_log_t;

typedef struct {
	/* Virtual UART
	 *   When there is no UART (e.g. Quickturn), the host should write a complete
	 *   input line directly into cbuf and then write the length into vcons_in.
	 *   This may also be used when there is a real UART (at risk of conflicting with
	 *   the real UART).  vcons_out is currently unused.
	 */
	volatile uint	vcons_in;
	volatile uint	vcons_out;

	/* Output (logging) buffer
	 *   Console output is written to a ring buffer log_buf at index log_idx.
	 *   The host may read the output when it sees log_idx advance.
	 *   Output will be lost if the output wraps around faster than the host polls.
	 */
	hnd_log_t	log;

	/* Console input line buffer
	 *   Characters are read one at a time into cbuf until <CR> is received, then
	 *   the buffer is processed as a command line.  Also used for virtual UART.
	 */
	uint		cbuf_idx;
	char		cbuf[CBUF_LEN];
} hnd_cons_t;

/* init/attach */
extern hnd_cons_t *hnd_cons_init(si_t *sih, osl_t *osh);
extern int hnd_cons_log_init(osl_t *osh);

/* Console command support */
hnd_cons_t *hnd_cons_active_cons_state(void);
extern void hnd_cons_check(void);
extern void hnd_cons_flush(void);
#if defined(HNDRTE_CONSOLE) || defined(BCM_OL_DEV)
extern void process_ccmd(char *line, uint len);
#endif /* HNDRTE_CONSOLE || BCM_OL_DEV */
typedef void (*cons_fun_t)(void *arg, int argc, char *argv[]);
#if defined(HNDRTE_CONSOLE) || defined(BCM_OL_DEV)
extern void hnd_cons_add_cmd(const char *name, cons_fun_t fun, void *arg);
#else
#define hnd_cons_add_cmd(name, fun, arg) { (void)(name); (void)(fun); (void)(arg); }
#endif /* HNDRTE_CONSOLE || BCM_OL_DEV */

/* receive callback to pass UART rx data to higher layers */
typedef void (*uart_rx_cb_t)(void *ctx, uint8 c);
void hndrte_uart_tx(uint8 *buf, int len);
void* hndrte_register_uart_rx_cb(void *ctx, uart_rx_cb_t cb);

#endif /* _hnd_cons_h_ */
