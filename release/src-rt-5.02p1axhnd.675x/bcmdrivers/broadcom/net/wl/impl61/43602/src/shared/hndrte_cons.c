/*
 * Console support for hndrte.
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
 * $Id: hndrte_cons.c 676784 2016-12-24 16:10:17Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <hndrte.h>
#include <bcmutils.h>
#include <siutils.h>
#include <sbchipc.h>
#include <hndchipc.h>
#include <bcmdevs.h>
#include <bcmsdpcm.h>
#include <bcmpcie.h>
#include <hndrte_debug.h>

#include <hndrte_cons.h>

#include <wlioctl.h>
#ifdef BCM_OL_DEV
#include <bcm_ol_msg.h>
#endif // endif
#if defined(RWL_DONGLE) || defined(UART_REFLECTOR)

#include <bcmcdc.h>
#include <rwl_shared.h>
#define REMOTE_REPLY 4
#define DATA_FRAG_LEN 960
#define REMOTE_SET_CMD 1
#define REMOTE_GET_CMD 2

extern hndrte_dev_t bcmwl;
#define RWL_HEADER_END		5		/* CDC header ends after 5 spaces */

static char rwl_buf[RWL_MAX_DATA_LEN];
static uint rwl_data_count;
static uint rwl_space_count;

static void remote_cmd_function(uint32 arg, uint argc, char *argv[]);
static void remote_uart_tx_packet(uchar* res_buf, int len);
void remote_uart_tx(uchar *buf);

/* Globals */
rwl_dongle_packet_t g_rwl_dongle_data;

#endif  /* RWL_DONGLE || UART_REFLECTOR */

#include <bcmstdlib.h>

#ifdef RWL_DONGLE
extern int g_rwl_dongle_flag;
#else
int g_rwl_dongle_flag = 0;
#endif // endif

/* Moved this out of RWL_DONGLE to make certain functions ROMable */
static void remote_reset_buffers(void);

typedef struct _serial_struct {
	int		baud_base;
	int		irq;
	unsigned char	*reg_base;
	unsigned short	reg_shift;
} serial_dev_t;

static serial_dev_t hndrte_uart;

/* serial_in: read a uart register */
static inline int
serial_in(serial_dev_t *dev, int offset)
{
	return ((int)R_REG(NULL, (uint8 *)(dev->reg_base + (offset << dev->reg_shift))));
}

/* serial_out: write a uart register */
static inline void
serial_out(serial_dev_t *dev, int offset, int value)
{
	W_REG(NULL, (uint8 *)(dev->reg_base + (offset << dev->reg_shift)), value);
}

/* serial_putc: spinwait for room in UART output FIFO, then write character */
static inline void
serial_putc(serial_dev_t *dev, int c)
{
	while ((serial_in(dev, UART_LSR) & UART_LSR_THRE))
		;
	serial_out(dev, UART_TX, c);
}

#ifndef EXT_CBALL
#ifdef HNDRTE_CONSOLE
/* serial_add: callback to initialize the uart structure */
static void
BCMATTACHFN(serial_add)(void *regs, uint irq, uint baud_base, uint reg_shift)
{
	int quot;

	if (hndrte_uart.reg_base)
		return;

	hndrte_uart.reg_base = regs;
	hndrte_uart.irq = irq;
	hndrte_uart.baud_base = baud_base / 16;
	hndrte_uart.reg_shift = reg_shift;

	/* Set baud and 8N1 */
	quot = (hndrte_uart.baud_base + 57600) / 115200;
	serial_out(&hndrte_uart, UART_LCR, UART_LCR_DLAB);
	serial_out(&hndrte_uart, UART_DLL, quot & 0xff);
	serial_out(&hndrte_uart, UART_DLM, quot >> 8);
	serial_out(&hndrte_uart, UART_LCR, UART_LCR_WLEN8);

	/* enable interrupts for rx data */
	serial_out(&hndrte_uart, UART_MCR, UART_MCR_OUT2);

	/* Enable PTIME (Programmable THRE Interrupt mode)
	 * enabled via the Interrupt Enable Register (IER[7])
	 * FIFOs need to be enabled (FCR[0] == 1)
	 * threshold level is programmed into FCR[5:4].
	 * The available empty thresholds are: empty, 2, 1/4 and 1/2.
	 * currently we use empty.
	 */
	serial_out(&hndrte_uart, UART_IER, UART_IER_ERBFI|UART_IER_ETBEI|UART_IER_PTIME);

	/* enable FIFOs */
	serial_out(&hndrte_uart, UART_FCR, UART_FCR_FIFO_ENABLE);

	/* According to the Synopsys website: "the serial clock
	 * modules must have time to see new register values
	 * and reset their respective state machines. This
	 * total time is guaranteed to be no more than
	 * (2 * baud divisor * 16) clock cycles of the slower
	 * of the two system clocks. No data should be transmitted
	 * or received before this maximum time expires."
	 */
	OSL_DELAY(1000);
}
#endif /* HNDRTE_CONSOLE */
#endif	/* EXT_CBALL */

/*
 * Console description
 */

typedef struct _ccmd {
	const char	*name;
	cons_fun_t	fun;
	uint32		arg;
	struct _ccmd	*next;
} ccmd_t;

typedef struct {
	/* Console state shared with host */
	hndrte_cons_t	state;

	/* UART device, NULL for none (virtual UART) */
	serial_dev_t	*uart;

	/* Console command processing */
	ccmd_t		*ccmd;

	/* enable/disable uart console output */
	bool	uart_console_output;

	/* echo mode per DVT test requirement */
#define ECHO_CMD "echo"
	bool echo_mode;

	/*
	 * receive callback to allow higher layer
	 * implementation to process UART data
	 */
	void		*uart_rx_cb_ctx;
	uart_rx_cb_t	uart_rx_cb;
} cons_soft_t;

/* Console is allocated dynamically for multiple console support. */
static cons_soft_t *cons0 = NULL;
static cons_soft_t *active_cons = NULL;

/* Forward decl */
void process_ccmd(char *line, uint len);
static char **process_cmdline(char *cmd_line, uint *argc);
static void hndrte_cons_isr(hndrte_dev_t *dev);
#if (defined(HNDRTE_CONSOLE) || defined(BCM_OL_DEV)) && !defined(BCM_BOOTLOADER)
void hndrte_cons_showcmds(uint32 arg, uint argc, char *argv[]);
#endif /* HNDRTE_CONSOLE || BCM_OL_DEV  && !BCM_BOOTLOADER */
static void serial_puts(cons_soft_t *cd);

/* serial_puts: Force output of characters until tx fifo is full or log buf is empty */
static void
serial_puts(cons_soft_t *cd)
{
	uint out_idx = cd->state.log.out_idx;

	if (!cd->uart_console_output)
		return;
	while ((out_idx != cd->state.log.idx) &&
		!(serial_in(cd->uart, UART_LSR) & UART_LSR_THRE)) {
		serial_out(cd->uart, UART_TX, cd->state.log.buf[out_idx]);
		out_idx = MODINC(out_idx, cd->state.log.buf_size);
	}
	cd->state.log.out_idx = out_idx;
}

static void *
BCMATTACHFN(add_console)(si_t *sih)
{
	if (active_cons == NULL) {
		if (cons0 == NULL)
			hndrte_log_init();
		active_cons = cons0;
	}

	if (active_cons == NULL)
		return NULL;

	active_cons->state.vcons_in = 0;
	active_cons->state.vcons_out = 0;
	active_cons->uart = NULL;

#ifdef BCMQT
	return active_cons;
#endif // endif

#ifdef DISABLE_CONSOLE_UART_OUTPUT
	active_cons->uart_console_output = FALSE;
#else
	active_cons->uart_console_output = TRUE;
#endif /* DISABLE_CONSOLE_UART_OUTPUT */
#ifdef HNDRTE_CONSOLE
#ifndef EXT_CBALL
	if ((sih->chippkg != HDLSIM_PKG_ID) && (sih->chippkg != HWSIM_PKG_ID))
		si_serial_init(sih, serial_add);
#endif /* !EXT_CBALL */

	if (hndrte_uart.reg_base)
		active_cons->uart = &hndrte_uart;

	/* dump whatever is already in the logbuf */
		if (active_cons->uart != NULL)
			serial_puts(active_cons);

#if !defined(BCM_OL_DEV) && !defined(BCM_BOOTLOADER)
	hndrte_cons_addcmd("?", hndrte_cons_showcmds, (uint32)active_cons);
#endif /* !BCM_OL_DEV  && !BCM_BOOTLOADER */
#endif /* HNDRTE_CONSOLE */
#if defined(RWL_DONGLE) || defined(UART_REFLECTOR)
	hndrte_cons_addcmd("rwl", remote_cmd_function, (uint32)active_cons);
#endif // endif

	return active_cons;
}

static void
hndrte_cons_isr(hndrte_dev_t *dev)	/* Argument dev not used */
{
	cons_soft_t *cd = active_cons;
	uint idx;

	/* Check for virtual UART input */
	if ((idx = cd->state.vcons_in) != 0) {
		int i;

		/* NUL-terminate */
		cd->state.cbuf[idx] = 0;

		/* Mark input consumed */
		cd->state.cbuf_idx = idx;

		/* echo it synchronously */
		for (i = 0; i < idx; i++)
			putc(cd->state.cbuf[i]);

		putc('\n');
	} else if (cd->uart == NULL)
		return;
	else {
		int iir_st, lsr, c;
#if defined(RWL_DONGLE) || defined(UART_REFLECTOR)
		static int new_line_flag;
#endif // endif

		/* Can read IIR only once per ISR */
		iir_st = serial_in(cd->uart, UART_IIR) & UART_IIR_INT_MASK;
		if (iir_st == UART_IIR_NOINT)
			return;

		/* Ready for next TX? */
		if (iir_st == UART_IIR_THRE) {
			/* Force output of characters until tx fifo is full or log buf is empty */
			serial_puts(cd);
		}

		/* Input available? */
		if (!((lsr = serial_in(cd->uart, UART_LSR)) & UART_LSR_RXRDY))
			return;

		/* Get the char */
		c = serial_in(cd->uart, UART_RX);

#if defined(RWL_DONGLE) || defined(UART_REFLECTOR)
		if (g_rwl_dongle_flag) {
		/* Receive error? */
			if (lsr & (UART_LSR_RX_FIFO | UART_LSR_BREAK |
				UART_LSR_FRAMING | UART_LSR_PARITY | UART_LSR_OVERRUN)) {
				remote_reset_buffers();
				cd->state.cbuf_idx = 0;
				return;
			}

			/* The data comes in the form of
			 * "rwl <cmd> <msg_length> <flag(shell or wl)>
			 * <datalength> <binarydata> \n\n".
			 * Hence each of the fields are separated by a space.
			 * (total 5 spaces). When the spaces count reaches more than
			 * or equal to 5 data is binary.
			 * rwl_data_count keeps track of how many number
			 * of bytes it has received.
			 * This data processing continues till two \n\n are received.
			 * This denotes end of transmission from client side.
			 * Now the data is ready to go to the server
			 */

			idx = cd->state.cbuf_idx;
			if (c != '\n') {
				/* Valid data; save it
				 * cbuf stores CDC header
				 */
				cd->state.cbuf[idx++] = c;
				/* Reset new line flag if we had received a \n earlier */
				new_line_flag = 0;

				if (rwl_space_count >= RWL_HEADER_END) {
					/* Store binary data coming after 5 spaces */
					rwl_buf[rwl_data_count] = c;
					rwl_data_count++;
				}
				if (c == ' ') {
					rwl_space_count++;
				}

				if ((cd->state.cbuf_idx = idx) < CBUF_LEN) {
					return;
				}
			} else if (c == '\n' && new_line_flag == 0) {
				/* If it is a newline upates the new_line_flag to 1.
				 * But this can be binary data to so store it for now
				 */
				cd->state.cbuf[idx++] = c;
				new_line_flag = 1;
				if (rwl_space_count >= RWL_HEADER_END) {
					rwl_buf[rwl_data_count] = c;
					rwl_data_count++;
				}

				if ((cd->state.cbuf_idx = idx) < CBUF_LEN) {
					return;
				}
			} else if (c == '\n' && new_line_flag == 1) {
				/* Now we received two consecutive \n.
				 * Decrement buffer to remove \n as it is not part of data
				 */
				cd->state.cbuf_idx--;
				idx = cd->state.cbuf_idx;
				cd->state.cbuf[idx] = '\0';
				 /* reset new line flag so as to recv fresh next time */
				new_line_flag = 0;
				/* \n\n is not part of the data. Remove that */
				if (rwl_space_count >= RWL_HEADER_END) {
					rwl_data_count--;
				}
			}
		}
#endif /* RWL_DONGLE || UART_REFLECTOR */

		if (cd->uart_rx_cb)
			cd->uart_rx_cb(cd->uart_rx_cb_ctx, (uint8)c);

		if (c == '\r')
			c = '\n';

		/* Backspace */
		if (c == '\b' || c == 0177) {
			if (cd->state.cbuf_idx > 0) {
				cd->state.cbuf_idx--;
				putc('\b');
				putc(' ');
				putc('\b');
			}
			return;
		}
		/* echo it synchronously */
		putc(c);

		/* per DVT testing requirement echo all characters
		 * entered after 'echo' command without
		 * storing/processing.
		 */
#if defined(WLTEST)
		if (cd->echo_mode && c != '\n') {
			return;
		}
#endif /* WLTEST */

		idx = cd->state.cbuf_idx;
		if (c != '\n') {
			/* Save it */
			cd->state.cbuf[idx++] = c;

			/* per DVT testing requirement look for 'echo' command */
#if defined(WLTEST)
			if ((idx == strlen(ECHO_CMD)) && (!cd->echo_mode) &&
				strncmp(cd->state.cbuf, ECHO_CMD, strlen(ECHO_CMD)) == 0) {
				cd->echo_mode = TRUE;
			}
#endif /* WLTEST */

			/* If not a carriage return, and still space in buffer;
			 * return from the poll to continue waiting.
			 */
			if ((cd->state.cbuf_idx = idx) < CBUF_LEN)
				return;
		} else
			cd->state.cbuf[idx] = '\0';
	}

	/* OK, process the input and call the proper command processor,
	 * but only if there is/was room for the terminating NUL.  Ignore
	 * (don't parse) a bad/unterminated string.
	 */
	if (1 &&
#if defined(WLTEST)
		!cd->echo_mode &&
#endif /* WLTEST */
		(cd->state.cbuf_idx = idx) < CBUF_LEN) {
		process_ccmd(cd->state.cbuf, idx);
	}

	/* After we are done, set up for next */
	cd->state.cbuf_idx = 0;
	cd->state.vcons_in = 0;
#if defined(WLTEST)
	cd->echo_mode = FALSE;
#endif /* WLTEST */

	putc('>');
	putc(' ');
}

static void
hndrte_uart_isr(void *cbdata, uint intstatus)
{
	hndrte_cons_isr((hndrte_dev_t *)cbdata);
}

/* hndrte_cons_init(sih): Initialize the console subsystem */
void
BCMATTACHFN(hndrte_cons_init)(si_t *sih)
{
	si_cc_register_isr(sih, hndrte_uart_isr, CI_UART, NULL);
	hndrte_uart.reg_base = NULL;
	add_console(sih);
}

/*
 * Called from sdpcmdev.c:sdpcmd_sendup in order to poll for virtual UART
 * input whenever the host sends the dongle a packet on the test channel.
 */
void
hndrte_cons_check(void)
{
	hndrte_cons_isr(NULL);
}

/*
 * Called to flush all and any pending UART data.
 */
void
hndrte_cons_flush(void)
{
	cons_soft_t *cd = active_cons;

	if ((cd->state.log.buf == NULL) || (cd->uart == NULL))
		return;

#ifdef HNDRTE_CONSOLE
	while (cd->state.log.idx != cd->state.log.out_idx) {
		serial_puts(cd);
	}
#endif // endif
}

/*
 * Console I/O:
 *	putc(c): Write out a byte to the console
 *	getc(): Wait for and read a byte from the console
 *	keypressed(): Check for key pressed
 */

void
putc(int c)
{
	cons_soft_t *cd = active_cons;

#if defined(RWL_DONGLE) || defined(UART_REFLECTOR)

	if (g_rwl_dongle_flag) {
		/* Safeguard against others using dongle for debug printfs */
		return;
	}
#endif // endif

	/* ready? */
	if (cd == NULL)
		return;

	/* CR before LF */
	if (c == '\n')
		putc('\r');

	if (cd->state.log.buf != NULL) {
		int next_idx = MODINC(cd->state.log.idx, cd->state.log.buf_size);

#ifdef HNDRTE_CONSOLE
#ifdef UART_LOSSLESS
		/* Detect if log buffer is almost full */
		if (MODINC(next_idx, cd->state.log.buf_size) == cd->state.log.out_idx) {
			/* Wait until there is space available in the uart tx fifo */
			while (serial_in(cd->uart, UART_LSR) & UART_LSR_THRE)
			;
		}
#else
		/* Detect if log buffer is full */
		if (next_idx == cd->state.log.out_idx) {
			/* Advance serial output index to 'oldest' character to print */
			cd->state.log.out_idx = MODINC(next_idx, cd->state.log.buf_size);
	}
#endif /* UART_LOSSLESS */
#endif /* HNDRTE_CONSOLE */

		/* Store in log buffer */
		cd->state.log.buf[cd->state.log.idx] = (char)c;
		cd->state.log.idx = next_idx;

#ifdef HNDRTE_CONSOLE
		if (cd->uart == NULL)
			return;
		/* Force output of characters until tx fifo is full or log buf is empty */
		serial_puts(cd);
#endif // endif
	}
}

bool
keypressed(void)
{
	cons_soft_t *cd = active_cons;

	/* Not initialized */
	if (cd == NULL)
		return FALSE;

	/* Virtual UART */
	if (cd->state.vcons_in != 0) {
		/* See if we have not yet consumed all the previous input */
		if (cd->state.cbuf_idx < cd->state.vcons_in)
			return TRUE;

		/* Out of input */
		cd->state.cbuf_idx = cd->state.vcons_in = 0;
	}

	/* Real UART */
	if (cd->uart != NULL)
		return (serial_in(cd->uart, UART_LSR) & UART_LSR_RXRDY) != 0;

	return FALSE;
}

int
getc(void)
{
	cons_soft_t *cd = active_cons;

	/* Not initialized */
	if (cd == NULL)
		return -1;

	for (;;) {
		/* Virtual UART */
		if (cd->state.vcons_in != 0) {
			/* See if we have not yet consumed all the previous input */
			if (cd->state.cbuf_idx < cd->state.vcons_in)
				return cd->state.cbuf[cd->state.cbuf_idx++];

			/* Out of input */
			cd->state.cbuf_idx = cd->state.vcons_in = 0;
		}

		/* Real UART */
		if (cd->uart != NULL) {
			if ((serial_in(cd->uart, UART_LSR) & UART_LSR_RXRDY) != 0)
				return serial_in(cd->uart, UART_RX);
		}

		hndrte_poll(hndrte_sih);
	}
}

#if defined(HNDRTE_CONSOLE) || defined(BCM_OL_DEV)
/* Routine to add an hndrte console command */
void
hndrte_cons_addcmd(const char *name, cons_fun_t fun, uint32 arg)
{
	cons_soft_t *cd = active_cons;
	ccmd_t *new;

	new = hndrte_malloc(sizeof(ccmd_t));
	ASSERT(new != NULL);

	new->name = name;
	new->fun = fun;
	new->arg = arg;
	new->next = cd->ccmd;

	cd->ccmd = new;
}
#endif /* HNDRTE_CONSOLE || BCM_OL_DEV */

/* Tokenize command line into argv */
static char **
process_cmdline(char *cmd_line, uint *argc)
{
	int pass2;
	char **argv = NULL, *s, *t;
	int qquote;

	/* Consumes input line by overwriting spaces with NULs */

	for (pass2 = 0; pass2 < 2; pass2++) {
		qquote = 0;
		for (s = cmd_line, *argc = 0;;) {
			while (*s == ' ')
				s++;
			if (!*s)
				break;
			if (pass2)
				argv[*argc] = s;
			(*argc)++;
			while (*s && (qquote || *s != ' ')) {
				if (*s == '"') {
					qquote ^= 1;
					if (pass2)	/* delete quotes 2nd pass */
						for (t = s--; (t[0] = t[1]) != 0; t++);
				}
				s++;
			}
			if (pass2 && *s)
				*s++ = 0;
		}

		if (!pass2) {
			argv = (char **)hndrte_malloc((*argc + 1) * sizeof(char *));
			ASSERT(argv);
		}
	}

	argv[*argc] = NULL;

	return argv;
}

void
process_ccmd(char *line, uint len)
{
#ifdef EVENT_LOG_ROM_PRINTF_MAP
	post_printf_hook printf_hook = NULL;
#endif // endif
	cons_soft_t *cd = active_cons;
	ccmd_t *ccmd = cd->ccmd;
	uint argc;
	char **argv = NULL;

	if (!line[0])
		return;

	argv = process_cmdline(line, &argc);
	if (argc == 0) {
		hndrte_free(argv);
		return;
	}

	ASSERT(argv[0]);

	while (ccmd != NULL) {
		if (strcmp(ccmd->name, argv[0]) == 0)
			break;

		ccmd = ccmd->next;
	}

#ifdef EVENT_LOG_ROM_PRINTF_MAP
	printf_hook = get_current_post_printf_hook();
	unregister_post_printf_hook();
#endif // endif

	if (ccmd != NULL)
		ccmd->fun(ccmd->arg, argc, argv);
	else {
#ifdef BCM_OL_DEV
		printf("Unknown command - %s!\n", line);
#else
		printf("?\n");
#endif // endif
	}

#ifdef EVENT_LOG_ROM_PRINTF_MAP
	register_post_printf_hook(printf_hook);
#endif // endif

	/* #ifdef Moved inside the function to make this function ROMable */
	remote_reset_buffers();

	hndrte_free(argv);
}

#if (defined(HNDRTE_CONSOLE) || defined(BCM_OL_DEV)) && !defined(BCM_BOOTLOADER)
void
hndrte_cons_showcmds(uint32 arg, uint argc, char *argv[])
{
	cons_soft_t *cd;
	ccmd_t *ccmd;

	if (arg == 0)
		cd = active_cons;
	else
		cd = (cons_soft_t *)arg;

	ccmd = cd->ccmd;

	while (ccmd != NULL) {
#ifdef BCMDBG
		printf("cmd \"%s\": %p(%p)\n", ccmd->name, (void *)ccmd->fun, (void *)ccmd->arg);
#else
		printf("%s\n", ccmd->name);
#endif // endif
		ccmd = ccmd->next;
	}
}
#endif /* HNDRTE_CONSOLE || BCM_OL_DEV */

int
BCMATTACHFN(hndrte_log_init)(void)
{
	if (cons0 != NULL)
		return BCME_ERROR;	/* already allocated */

	cons0 = (cons_soft_t *)hndrte_malloc(sizeof(cons_soft_t));
	if (cons0 == NULL)
		return BCME_ERROR;

	bzero(cons0, sizeof(cons_soft_t));

	cons0->state.log.buf = (char *)hndrte_malloc(LOG_BUF_LEN);
	if (cons0->state.log.buf == NULL) {
		hndrte_free((void *)cons0);
		cons0 = NULL;
		return BCME_ERROR;
	}

	cons0->state.log.buf = (char *)OSL_UNCACHED(cons0->state.log.buf);
	cons0->state.log.buf_size = LOG_BUF_LEN;

	bzero(cons0->state.log.buf, LOG_BUF_LEN);

	active_cons = cons0;

#if defined(BCMPCIEDEV_ENABLED)
	pciedev_shared.console_addr = (uint32)&active_cons->state;
#endif // endif
#if defined(BCMSDIODEV_ENABLED)
	sdpcm_shared.console_addr = (uint32)&active_cons->state;
#endif // endif
#if defined(BCM_OL_DEV)
	ppcie_shared->console_addr = (uint32)&active_cons->state.log;
#endif // endif

	return 0;
}

hndrte_cons_t *
hndrte_get_active_cons_state(void)
{
	if (active_cons == NULL)
		return NULL;
	return &active_cons->state;
}

void
hndrte_uart_tx(uint8 *buf, int len)
{
	cons_soft_t *cd = active_cons;
	int i = 0;

	if (!buf)
		return;

	if (cd->uart) {
		for (i = 0; i < len; i++) {
			serial_putc(cd->uart, buf[i]);
		}
	}
}

void *
hndrte_register_uart_rx_cb(void *ctx, uart_rx_cb_t cb)
{
	cons_soft_t *cd = active_cons;

	cd->uart_rx_cb = cb;
	cd->uart_rx_cb_ctx = ctx;

	return cd;
}

static void
remote_reset_buffers(void)
{
#if defined(RWL_DONGLE) || defined(UART_REFLECTOR)
	memset(rwl_buf, 0, RWL_MAX_DATA_LEN);
	rwl_data_count = 0;
	rwl_space_count = 0;
#endif // endif
}

#if defined(RWL_DONGLE) || defined(UART_REFLECTOR)

/* Function is mapped to 'rwl' command
 * Function constructs the CDC header from command line arguments, calculates
 * the packet length. Copies CDC header and data in the global buffer shared
 * between UART driver and wl driver and indiactes wl driver that packet is ready to
 * be picked up
 */
static void
remote_cmd_function(uint32 arg, uint argc, char *argv[])
{
	rem_ioctl_t rem_cdc;

#ifdef UART_REFLECTOR
	int err;
	uint noframes;
	uint count;
	rem_ioctl_t *reply_cdc = NULL;
	uchar *buffer = NULL;
	uchar *ptr = NULL;
	uint rem_bytes = 0;
	g_rwl_dongle_flag = 1;
	rem_ioctl_t reply;
	const char err_msg[] = "In-dongle does not support shell/ASD/DHD\n";
#endif // endif
	rem_cdc.msg.cmd = atoi(argv[1]);
	rem_cdc.msg.len   = atoi(argv[2]);
	rem_cdc.msg.flags = atoi(argv[3]);

	rem_cdc.data_len = rwl_data_count;

	g_rwl_dongle_data.packet_len = sizeof(rem_ioctl_t) + rem_cdc.data_len;

	/* Allocates memory for the packet */
	g_rwl_dongle_data.packet_buf = (uchar*)MALLOC(NULL, g_rwl_dongle_data.packet_len);

	/* Copy the CDC header */
	bcopy((char*)(&rem_cdc), g_rwl_dongle_data.packet_buf, sizeof(rem_ioctl_t));

	/* Copy the binary data */
	bcopy(rwl_buf, &g_rwl_dongle_data.packet_buf[sizeof(rem_ioctl_t)], rwl_data_count);

#ifdef UART_REFLECTOR
	/* The bcmwl structure is a wl driver handle that is created at
	 * the initialization of the dongle. It is defined in sys/wl_rte.c
	 * We use that handle to call the wlc_ioctls to get from or set to driver
	 */
	if (rem_cdc.msg.flags & REMOTE_SET_CMD) {
	/* Capture the packet, call WLC ioctl, send back the response
	 * We call wl_ioctl using the pointer from the bcmwl structure
	 */
		err = bcmwl.funcs->ioctl(&bcmwl, rem_cdc.msg.cmd,
		                         &g_rwl_dongle_data.packet_buf[REMOTE_SIZE],
		                         rem_cdc.msg.len, NULL, NULL, FALSE);
		reply.msg.cmd = err;
		reply.msg.len = 0;
		reply.msg.flags = REMOTE_REPLY;
		reply.data_len = 0;
		remote_uart_tx((uchar *)&reply);
	}
	else if (rem_cdc.msg.flags & REMOTE_GET_CMD) {
		/* Allocate buffer for ioctl results */
		buffer = (uchar *)MALLOC(NULL, rem_cdc.msg.len+REMOTE_SIZE);
		memcpy(&buffer[REMOTE_SIZE],
			&g_rwl_dongle_data.packet_buf[REMOTE_SIZE], rwl_data_count);
		err = bcmwl.funcs->ioctl(&bcmwl, rem_cdc.msg.cmd,
		                         &buffer[REMOTE_SIZE], rem_cdc.msg.len, NULL, NULL, FALSE);

		reply.msg.cmd = err;
		reply.msg.len = rem_cdc.msg.len;
		reply.msg.flags = REMOTE_REPLY;
		reply.data_len = rem_cdc.data_len;

		if (rem_cdc.msg.len < DATA_FRAG_LEN) {
			/* Response size fits into single packet.
			 * Send it off
			 */
			memcpy(buffer, &reply, REMOTE_SIZE);
			remote_uart_tx(buffer);
		}
		 else {
			ptr = buffer;
			noframes = rem_cdc.msg.len/DATA_FRAG_LEN;
			if ((rem_bytes = (rem_cdc.msg.len%DATA_FRAG_LEN)) > 0) {
				noframes++;
			}
			/* Fragment and sent out the frames */
			for (count = 0; count < noframes; count++) {
				if (count == noframes -1)
					reply.data_len = rem_bytes;
				else
					reply.data_len = DATA_FRAG_LEN;
				memcpy(ptr, &reply, REMOTE_SIZE);
				remote_uart_tx(ptr);
				ptr += DATA_FRAG_LEN;
			}

		}
		MFREE(NULL, buffer, rem_cdc.msg.len+REMOTE_SIZE);
	} else
	{
		/* Invalid wl command received. Inform the client that
		 * the command cannot be executed
		 */
		reply_cdc = (rem_ioctl_t*)&g_rwl_dongle_data.packet_buf[0];
		reply_cdc->msg.cmd = 0;
		reply_cdc->msg.flags = REMOTE_REPLY;
		strcpy((char*)&g_rwl_dongle_data.packet_buf[REMOTE_SIZE],
			err_msg);
		remote_uart_tx(g_rwl_dongle_data.packet_buf);
	}
	/* Free the allocated memory for the packet. When reflector is not enabled
	 * this is done when application reads out the data.
	 */
	MFREE(NULL, g_rwl_dongle_data.packet_buf, g_rwl_dongle_data.packet_len);

#endif /* UART_REFLECTOR */
	/* Clear the buffers since we have the copied data in global buffers */
	remote_reset_buffers();
#ifndef UART_REFLECTOR
	/* If UART_REFLECTOR is not defined, then indicate that we have a packet
	 * else we use and modify the input buffers to send out the response and
	 * thus the input buffers get destroyed. We donot want to send destroyed
	 * buffer to the application layer
	 */
	g_rwl_dongle_data.packet_status = 1;
#endif // endif

}

/* Function gets the packet(buf) from wl driver
 *  Extracts the CDC header and transmits the header
 *  and then transmits the data by calling respective functions.
 */
void
remote_uart_tx(uchar *buf)
{
	rem_ioctl_t rem_cdc;

	bcopy(buf, &rem_cdc, sizeof(rem_ioctl_t));

	remote_uart_tx_packet((uchar*)&rem_cdc, sizeof(rem_ioctl_t));

	if (rem_cdc.data_len != 0) {
		remote_uart_tx_packet(&buf[sizeof(rem_ioctl_t)], rem_cdc.data_len);
	}
}

static void
remote_uart_tx_packet(uchar* res_buf, int len)
{
	int i;
	cons_soft_t *cd = active_cons;

	bcopy((char *)res_buf, cd->state.log.buf, len);

	if (cd->uart != NULL)
		for (i = 0; i < len; i++)
			serial_putc(cd->uart, cd->state.log.buf[i]);
}
#endif  /* RWL_DONGLE || UART_REFLECTOR */
