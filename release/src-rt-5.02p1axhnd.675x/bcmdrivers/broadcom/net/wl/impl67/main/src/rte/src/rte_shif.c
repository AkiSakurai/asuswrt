/*
 * API Implementation to communicate with Sensor Hub chip using
 * debug UART APIs.
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
 * $Id: $
 */

#include <bcm_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <rte_shif.h>
#include <rte_cons.h>
#include <rte_uart.h>
#include <rte_timer.h>
#include <rte_chipc.h>
#include <hndpmu.h>
#include <hndsoc.h>
#include <rte.h>
#include <sbgci.h>

#if !defined(BCMDONGLEHOST) && !defined(BCM_BOOTLOADER) && defined(SR_ESSENTIALS)
#include <saverestore.h>
#endif /* !BCMDONGLEHOST && !BCM_BOOTLOADER && SR_ESSENTIALS */

#define MAX_PINBLOCK   3

#ifdef GCI_UART
#include "gci_uart_priv.h"
#endif /* GCI_UART */

#ifdef RTESHIF_DBG
#define SHIF_DBG(args) printf args
#define SHIF_INFO(args) printf args
#else
#define SHIF_INFO(args)
#define SHIF_DBG(args)
#endif  /* RTESHIF_DBG */

#ifndef RTE_SHIF_UART_ID
#ifdef GCI_UART
#define RTE_SHIF_UART_ID	GCI_UART_ID
#else
#define RTE_SHIF_UART_ID	0
#endif /* GCI_UART */
#endif /* RTE_SHIF_UART_ID */

#define SHIF_TX_BUF_SIZE        150
#define	SHIF_RX_BUF_SIZE	150
#define	WP_WAKE_REQ	0xAB
#define	WP_WAKE_RESP	0xBA

typedef enum {
	WP_SLEEP	= 1,
	WP_IDLE		= 2,
	WP_ACTIVE	= 3,
	WP_DISABLE	= 4
} shif_wp_state_t;

typedef struct shif_rx_ctxt {
	char	delim;	/* indicates end of rx stream */
	int	len;	/* client requested byte count */
	int	timeout;
	uint8	*buf;
	uint16	bytes_rxd;
	/* rx callback ctx */
	rx_cb_t	rx_cb;
	void	*rx_cb_param;
} shif_rx_ctxt_t;

typedef struct shif_wp {
	si_t	*sih;
	osl_t	*osh;
	shif_wp_state_t	state;	/* current state */
	bool	shif_4_wire;	/* to extend the support for 2-wire interface */
	hnd_timer_t *timer;	/* to track inactivity */
	uint16	timeout;	/* time in ms to wait */
	bool	timer_active;
	void*	wake_handle;
	/* to support 2-wire interface */
	uint8	wake_req;
	uint8	wake_resp;
	char	*tx_buf; /* temp buffer */
	uint16	tx_len;
	void	*gci_wake_handle;
	uint32	clk_sts_reg;
	uint8	sh_pin;
	uint8	int_pin;
	uint16  pkt_type;
	uint8	xstate;
	tx_status_cb_t	tx_status_cb;
	void	*tx_status_cb_param;
	bool	wake_intr_status;
	shif_info_t	*shif_p;
} shif_wp_t;

struct shif_info {
	si_t		*sih;
	osl_t		*osh;
	shif_rx_ctxt_t	rx_ctxt;
	shif_wp_t	*wp_p;

	serial_dev_t	*uart;
	hnd_timer_t	*gci_uart_tx_timer; /* zero timer */
	void		*uart_rx_cb_ctx;
	uart_rx_cb_t	uart_rx_cb;
	char		*tx_buf;
	int		tx_count;
	bool shif_enable;
};

#define	WP_EVT_TX	1
#define	WP_EVT_RX	2
#define	WP_EVT_TO	3
#define	WP_EVT_LOW	4
#define WP_EVT_RX_CPLT	5
#define WP_EVT_HIGH	6
#define WP_EVT_RX_WAIT	7

#ifdef RTESHIF_DBG
static char *event_strings[] = {
	"null",
	"tx",
	"rx",
	"timeout",
	"int_low",
	"rx complete",
	"int_high",
	"rx wait"
};

static char *tx_state_strings[] = {
	"TX_ALLOWED",
	"TX_WAITING",
	"TX_FAILED",
	"TX_COMPLETE",
	"RX_WAITING",
	"RX_FAILED",
	"RX_COMPLETE"
};

static char *state_strings[] = {
	"null",
	"sleep",
	"idle",
	"active"
};
#endif /* SHIF_DEBUG */

#define	IS_WAKE_REQ(p, c)	((p)->wake_req == c)
#define	IS_WAKE_RESP(p, c)	((p)->wake_resp == c)

int pinblock_cnt = 0;

static void rte_shif_uart_rx(void *ctx, uint8 c);
static int rte_shif_send_to_shif(shif_info_t *shif_p, char *buf, uint16 len);
static void rte_shif_wp_config_wake_pins(shif_wp_t *wp_p);
static void rte_shif_wp_free_wake_pins(shif_wp_t *wp_p);
static void _wp_event(shif_wp_t *wp_p, uint8 evt);
static bool rte_shif_uart_worklet(void *cbdata);
static void rte_shif_uart_tx_timer_handler(hnd_timer_t *t);
static int rte_shif_uart_tx(shif_info_t *shif_p, uint8 *buf, int len);
static void rte_shif_update_txstatus(shif_wp_t *wp_p, uint8 txstatus);
static void rte_shif_update_rxpkt(shif_wp_t *wp_p);
static void rte_shif_interrupt_enable(shif_wp_t *wp_p, bool enable);
static shif_wp_t* rte_shif_wp_init(shif_info_t *shif_p, si_t *sih, osl_t *osh,
		uint8 sh_pin, uint8 int_pin);
static void _wp_event_sleep(shif_wp_t *wp_p, uint8 evt);
static void _wp_event_idle(shif_wp_t *wp_p, uint8 evt);
static void _wp_event_active(shif_wp_t *wp_p, uint8 evt);

#ifdef RTESHIF_DBG
void reg_dump(shif_wp_t *wp_p);
#endif // endif

/* receive callback to pass UART rx data to higher layers */
typedef void (*uart_rx_cb_t)(void *ctx, uint8 c);
void* rte_register_shif_rx_cb(void *ctx, uart_rx_cb_t cb);
void rte_shif_uart_flush(void *cbdata);

/* puts data into UART synchronously */
static int
_wp_send_data(shif_wp_t *wp_p)
{
	int ret = BCME_OK;

	if (!(wp_p->tx_buf && wp_p->tx_len)) {
		SHIF_DBG(("%s: data sending fail\n", __FUNCTION__));
		return BCME_ERROR;
	}

	ret = rte_shif_send_to_shif(wp_p->shif_p, wp_p->tx_buf,
			wp_p->tx_len);

	MFREE(wp_p->osh, wp_p->tx_buf, wp_p->tx_len);
	wp_p->tx_buf = NULL;
	wp_p->tx_len = 0;

	return ret;
}

static void
_wp_event(shif_wp_t *wp_p, uint8 evt)
{
	switch (wp_p->state) {
		case WP_SLEEP:
			{
				_wp_event_sleep(wp_p, evt);
				break;
			}
		case WP_IDLE:
			{
				_wp_event_idle(wp_p, evt);
				break;
			}
		case WP_ACTIVE:
			{
				_wp_event_active(wp_p, evt);
				break;
			}
		default:
			break;
	}

	if (wp_p->xstate == SHIF_TX_COMPLETE || wp_p->xstate == SHIF_TX_FAILED) {
		rte_shif_update_txstatus(wp_p, wp_p->xstate);
	}
}

static void
_wp_timer_callback(hnd_timer_t *t)
{
	shif_wp_t *wp_p = (shif_wp_t *)hnd_timer_get_data(t); //->data;

	SHIF_INFO(("%s:state %x tx len %d\n", __FUNCTION__, wp_p->state,
			wp_p->tx_len));

	_wp_event(wp_p, WP_EVT_TO);

	/* XXX debug uart handles tx synchronously.
	 * need additional logic here to handle tx
	 * if tx becomes asynchronous(like in gci)
	 */
}

/* rte_shif_send return value description
 *  When there is no error, return wp_p->xstate
 *  When there is an error, return BCME error state except BCME_OK
 */
int
rte_shif_send(shif_info_t *shifp, char *buf, uint16 pkt_type, bool retry, uint16 len)
{
	shif_wp_t *wp_p;
	int ret = BCME_OK;
	int old_txstatus;

	if (!shifp) {
		SHIF_DBG(("%s: shifp NULL return\n", __FUNCTION__));
		ret = BCME_ERROR;
		goto exit;
	}

	if (!buf) {
		SHIF_DBG(("%s: buf is NULL\n", __FUNCTION__));
		ret = BCME_ERROR;
		goto exit;
	}

	wp_p = shifp->wp_p;

	if (wp_p == NULL) {
		SHIF_DBG(("%s: Error: shif was not initialized\n", __FUNCTION__));
		ret = BCME_ERROR;
		goto exit;
	}

	if (wp_p->state == WP_DISABLE) {
		ret = BCME_BUSY;
		SHIF_DBG(("%s: shif interface disabled\n", __FUNCTION__));
		goto exit;
	}

	SHIF_INFO(("%s: pkt_type %d, retry %d, Tx state %s\n",
			_FUNCTION__, pkt_type, retry,
			tx_state_strings[shifp->wp_p->xstate]));
	if (wp_p->xstate != SHIF_TX_ALLOWED) {
		if (wp_p->xstate == SHIF_TX_WAITING) {
			ret = SHIF_TX_WAITING;
		}
		else if (wp_p->xstate == SHIF_RX_WAITING) {
			ret = SHIF_RX_WAITING;
		}
		else {
			if (retry) {
				SHIF_INFO(("%s: retry packet process\n", __FUNCTION__));
				old_txstatus = wp_p->xstate;
				wp_p->xstate = SHIF_TX_ALLOWED;
				ret = old_txstatus;
			}
			else {
				SHIF_INFO(("%s: new packet process\n", __FUNCTION__));
				/* previous tx result does not affect new tx(not retry) */
			}
		}
	}

	if (wp_p->xstate == SHIF_TX_ALLOWED) {
		wp_p->pkt_type = pkt_type;
	} else {
		SHIF_INFO(("%s: previous packet is not completed, pkt_type %x\n",
				__FUNCTION__, wp_p->pkt_type));
		ret = BCME_ERROR;
	}

	if (ret != BCME_OK) {
		goto exit;
	}

	wp_p->xstate = SHIF_TX_WAITING;

	if (wp_p->shif_4_wire) {
		ASSERT(wp_p->tx_buf == NULL);
		wp_p->tx_buf = buf;
		wp_p->tx_len = len;
		_wp_event(wp_p, WP_EVT_TX);
		ret = wp_p->xstate;

		return ret;
	} else {
		ret = rte_shif_send_to_shif(shifp, buf, len);
	}

exit:
	if (buf && shifp != NULL) {
		MFREE(shifp->osh, buf, len);
	}

	return ret;
}

static shif_wp_t *
rte_shif_wp_init(shif_info_t *shif_p, si_t *sih, osl_t *osh,
		uint8 sh_pin, uint8 int_pin)
{
	shif_wp_t *wp_p;

	/* allocate wp context */
	wp_p = MALLOCZ(osh, sizeof(shif_wp_t));
	if (!wp_p) {
		SHIF_DBG(("%s: No memory!\n", __FUNCTION__));
		return NULL;
	}

	wp_p->shif_p = shif_p;
	wp_p->sih = sih;
	wp_p->osh = osh;
	wp_p->state = WP_SLEEP;

	wp_p->timer = hnd_timer_create((void *)sih, wp_p,
			_wp_timer_callback, NULL, NULL);
	if (!wp_p->timer) {
		SHIF_DBG(("%s: timer creation failed!\n", __FUNCTION__));
		goto error;
	}
	wp_p->timeout	= SHIF_AWAKE_TIMEOUT;

	/* to use with 2-wire interface */
	wp_p->wake_req = WP_WAKE_REQ;
	wp_p->wake_resp = WP_WAKE_RESP;

	wp_p->shif_4_wire =  TRUE;

	wp_p->sh_pin = sh_pin;
	wp_p->int_pin = int_pin;
	wp_p->pkt_type = SHIF_PKT_NOTDEFINED;

	rte_shif_wp_config_wake_pins(wp_p);

	return wp_p;
error:
	MFREE(osh, wp_p, sizeof(shif_wp_t));
	return NULL;
}

static void
rte_shif_wp_deinit(shif_wp_t *wp_p)
{
	if (wp_p->timer) {
		hnd_timer_stop(wp_p->timer);
	}

	rte_shif_wp_free_wake_pins(wp_p);

	if (wp_p->tx_buf) {
		MFREE(wp_p->osh, wp_p->tx_buf, wp_p->tx_len);
	}

	/* free wp context */
	MFREE(wp_p->osh, wp_p, sizeof(shif_wp_t));
}

int
rte_shif_wp_enable(shif_info_t *shifp, int enable, uint8 sh_pin, uint8 int_pin)
{
	if (enable) {
		/* init wp if not initialized */
		if (!shifp->wp_p) {
			shifp->wp_p = rte_shif_wp_init(shifp, shifp->sih, shifp->osh,
					sh_pin, int_pin);
		}
	} else {
		if (shifp->wp_p) {
			rte_shif_wp_deinit(shifp->wp_p);
		}

		shifp->wp_p = NULL;
	}
	return BCME_OK;
}

/*
 * For initial demo allow direct transmission bypassing
 * power save state machine
 */
static int
rte_shif_send_to_shif(shif_info_t *shif_p, char *buf, uint16 len)
{

	shif_info_t *cd = shif_p;
	int ret = BCME_OK;

	if (cd == NULL) {
		SHIF_DBG(("%s: tx state : context is NULL\n", __FUNCTION__));
		ret = BCME_ERROR;
		goto exit;
	}

	if (!buf) {
		SHIF_DBG(("%s: tx state : BUFTOOSHORT\n", __FUNCTION__));
		ret = BCME_BUFTOOSHORT;
		goto exit;
	}

	if (cd->uart && cd->tx_count) {
		SHIF_DBG(("%s: tx state : BUSY\n", __FUNCTION__));
		ret = BCME_BUSY;
	} else {
		ret = rte_shif_uart_tx(shif_p, (uint8 *)buf, len);

#ifdef RTESHIF_DBG
		char tx_string[PKTLOG_TLEN] = "Tx: ";
		int i, j;

		for (i = PKTLOG_HLEN, j = 0; j < len && i <= PKTLOG_TLEN;
				i += PKTLOG_CLEN, j++) {
			sprintf(&tx_string[i], "%2x ", buf[j]);
		}
		SHIF_INFO(("%s\n", tx_string));
#endif /* RTESHIF_DBG */
	}

exit:

	return ret;
}

/*
 *	NFC receive function
 */
uint8
rte_shif_get_rxdata(shif_info_t *shif_p, char *buf, uint8 count)
{
	shif_rx_ctxt_t *rx_ctx_p;
	int bytes_to_copy = 0;

	ASSERT(shif_p);
	rx_ctx_p = &shif_p->rx_ctxt;

	if (rx_ctx_p->bytes_rxd > 0) {
		bytes_to_copy = (count < rx_ctx_p->bytes_rxd)? count :
			rx_ctx_p->bytes_rxd;
		memcpy(buf, rx_ctx_p->buf, bytes_to_copy);
	}

	rx_ctx_p->bytes_rxd = 0;

	return bytes_to_copy;
}

/*
 *	receive handler
 */
static void
rte_shif_uart_rx(void *ctx, uint8 ch)
{
	shif_info_t	*shif_p;
	shif_rx_ctxt_t	*rx_ctx_p;
	uint16		*bytes_rxd;
	bool		rx_done = FALSE;
	char		c = ch;

	ASSERT(ctx);
	shif_p = (shif_info_t *)ctx;
	rx_ctx_p = &shif_p->rx_ctxt;
	bytes_rxd = &rx_ctx_p->bytes_rxd;

	/* let wp validate received data */

	if (shif_p->wp_p == NULL) {
		SHIF_DBG(("%s: RX failed. wake pin error\n", __FUNCTION__));
		return;
	}

	*bytes_rxd %= SHIF_RX_BUF_SIZE;
	rx_ctx_p->buf[(*bytes_rxd)++] = c;

	if (rx_ctx_p->delim != SHIF_DELIM_NOTDEFINED &&
			rx_ctx_p->delim == c) {
		/* delim received */
		rx_done = TRUE;
	}

	if (rx_ctx_p->len != SHIF_LEN_NOTDEFINED &&
			rx_ctx_p->len == *bytes_rxd) {
		/* expected byte count reached */
		rx_done = TRUE;
	}

	if (rx_done && rx_ctx_p->rx_cb) {
		SHIF_INFO(("%s: rx_done, shif state %s\n", __FUNCTION__,
				tx_state_strings[shif_p->wp_p->xstate]));
		/*
		*If rx is done, firstly WAKE pin state has to be changed to SLEEP state
		*for blocking packet tx on RX working
		*/

		if (shif_p->wp_p->xstate == SHIF_RX_WAITING) {
			_wp_event(shif_p->wp_p, WP_EVT_RX_CPLT);
		}
		else {
			SHIF_DBG(("%s: discard rx data for No sync recv\n", __FUNCTION__));
		}
	}
}

/* _wp_wake_handler
 * when DATA recv sequence is
 * pre condition. SensorHub_to_WLAN interrupt pin is deactive HIGH (active LOW)
 * 1. SensorHub wake WLAN by low level interrupt and keep LOW signal
 * (not falling edge, In 4359(4349) chip test, It does not be waked by edge interrupt)
 * 2. SensorHub send uart data to WLAN
 * 3. SensorHub notify uart data seding completion of WLAN by rising edge interrupt
 * and keep HIGH signal
 */
static void
_wp_wake_handler(uint32 status, void *arg)
{
	shif_wp_t *wp_p;

	SHIF_DBG(("%s: status %x\n", __FUNCTION__, status));

	ASSERT(arg);

	wp_p = (shif_wp_t *)arg;

	if (wp_p->state == WP_DISABLE) {
		SHIF_DBG(("%s:shif interface disabled\n", __FUNCTION__));
		return;
	}

	/* In 4359 test, chip did not waked by falling edge,
	 * So It changed to LOW level interrupt. SensorHub_to_WLAN interrtup is
	 * keep LOW signal for recieving UART data. In this time, LOW level
	 * interrupt has to be disabled becuase of LOW level interrupt continuous
	 * rte_shif_interrupt_enable() is only for disable
	 * LOW level interrupt. It does not disable interrupt by rising edge
	 */

	if (status & (1 << GCI_GPIO_STS_VALUE_BIT)) {
		/* blocking low_level interrupt on handling */
		if (wp_p->wake_intr_status == FALSE) {
			SHIF_DBG(("%s: on wake interrupt progress\n", __FUNCTION__));
			return;
		}
		/* disable LOW level wake interrupt */
		wp_p->wake_intr_status = FALSE;
		rte_shif_interrupt_enable(wp_p, wp_p->wake_intr_status);

		_wp_event(wp_p, WP_EVT_LOW);

	} else {
		_wp_event(wp_p, WP_EVT_HIGH);

		/* enable LOW level wake interrupt */
		wp_p->wake_intr_status = TRUE;
		rte_shif_interrupt_enable(wp_p, wp_p->wake_intr_status);
	}
}

static void
rte_shif_wp_config_wake_pins(shif_wp_t *wp_p)
{
	si_t	*sih = wp_p->sih;
	uint32	sh_wake_pin_mask = 0;
	uint32	sh_wake_int_pin_mask = 0;
	uint8	wake_sh_pin = wp_p->sh_pin;
	uint8	wake_int_pin = wp_p->int_pin;

	SHIF_INFO(("%s: sh wake pin %d, wlan wake int pin %d\n", __FUNCTION__,
			wp_p->sh_pin, wp_p->int_pin));

	wp_p->wake_intr_status = TRUE;

	switch (CHIPID(sih->chip)) {
		case BCM4347_CHIP_GRPID :
			{
				si_gci_set_functionsel(sih, wake_sh_pin, CC4347_FNSEL_SAMEASPIN);
				si_gci_set_functionsel(sih, wake_int_pin, CC4347_FNSEL_GCI1);
				break;
			}
		default:;
	}

	/*
	 * configure wake_sh pin
	 */
	sh_wake_pin_mask |= (1 << wake_sh_pin);
	sh_wake_int_pin_mask |= (1 << wake_int_pin);
	si_gpiocontrol(sih, sh_wake_pin_mask, 0, GPIO_HI_PRIORITY);
	/* bug fix: when wifi on, sh_wake pin is shaked
	 * After outen setting, gpio level is not set exactly to wanted
	 * So level setting have to be before outen setting
	 */
	si_gpioout(sih, sh_wake_pin_mask, sh_wake_pin_mask, GPIO_HI_PRIORITY);
	si_gpioouten(sih, sh_wake_pin_mask, sh_wake_pin_mask, GPIO_HI_PRIORITY);

	/* enable weak pull-up on wake_int_pin */
	si_gpiopull(sih, FALSE, sh_wake_int_pin_mask, sh_wake_int_pin_mask);
	si_gpiopull(sih, FALSE, sh_wake_pin_mask, 0);

	/*
	 * register handler for wlan wake interrupt
	 * gpioX is used as wake interrupt
	 */
#ifdef USE_CC_GPIO
	si_gpio_handler_register(sih, sh_wake_int_pin_mask, FALSE,
			_wp_wake_handler, (void *)wp_p);

	si_gpioevent(sih, GPIO_REGEVT_INTMSK, sh_wake_int_pin_mask,
			sh_wake_int_pin_mask);
	BCM_REFERENCE(wake_events);
#endif // endif
}

static void
rte_shif_wp_free_wake_pins(shif_wp_t *wp_p)
{
	si_t *sih = wp_p->sih;
	uint8	wake_sh_pin = wp_p->sh_pin;
	uint8	wake_int_pin = wp_p->int_pin;
	uint32	sh_wake_pin_mask;
	uint32	sh_wake_int_pin_mask;

	sh_wake_pin_mask = 1 << wake_sh_pin;
	sh_wake_int_pin_mask = 1 << wake_int_pin;

	/* enable weak pull-up on wake_int_pin */
	si_gpioouten(sih, sh_wake_pin_mask, 0, GPIO_HI_PRIORITY);
	si_gpiopull(sih, FALSE, sh_wake_int_pin_mask, sh_wake_int_pin_mask);
	si_gpiopull(sih, FALSE, sh_wake_pin_mask, sh_wake_pin_mask);

	si_gci_free_wake_pin(sih, wake_int_pin);
}

int
rte_shif_up_fn(shif_info_t *shifp)
{
	int ret = BCME_OK;

	ASSERT(shifp);

	if (shifp && shifp->shif_enable) {
		rte_shif_wp_config_wake_pins(shifp->wp_p);
	} else {
		ret = BCME_ERROR;
	}

	return ret;
}

int
rte_shif_config_rx_completion(shif_info_t *shif_p, char delim, int len,
		int timeout, rx_cb_t rx_cb, void *param)
{
	ASSERT(shif_p);

	shif_p->rx_ctxt.delim = SHIF_DELIM_NOTDEFINED;
	shif_p->rx_ctxt.len = SHIF_DELIM_NOTDEFINED;
	shif_p->rx_ctxt.timeout = 0;

	shif_p->rx_ctxt.delim = delim;
	if (delim != SHIF_DELIM_NOTDEFINED) {
		shif_p->rx_ctxt.delim = delim;
	} else if (len != SHIF_DELIM_NOTDEFINED) {
		shif_p->rx_ctxt.len = len;
	}

	shif_p->rx_ctxt.timeout = 0;

	shif_p->rx_ctxt.rx_cb = rx_cb;
	shif_p->rx_ctxt.rx_cb_param = param;
	shif_p->wp_p->tx_status_cb_param = param;

	/* allocate local buffer */
	shif_p->rx_ctxt.buf = MALLOCZ(shif_p->osh, SHIF_RX_BUF_SIZE);
	if (!shif_p->rx_ctxt.buf) {
		SHIF_DBG(("%s No memory \n", __FUNCTION__));
		return BCME_NOMEM;
	}

	shif_p->rx_ctxt.bytes_rxd = 0;

	rte_register_shif_rx_cb(shif_p, rte_shif_uart_rx);

	return BCME_OK;
}

void
rte_shif_uart_flush(void *cbdata)
{
	shif_info_t *cd = (shif_info_t *)cbdata;
	int mask;

	if ((cd->tx_buf == NULL) || (cd->uart == NULL))
		return;

	if (cd->tx_count != 0) {
		mask = si_gci_direct(cd->sih, GCI_OFFSETOF(cd->sih, gci_intmask), 0, 0);
		si_gci_direct(cd->sih, GCI_OFFSETOF(cd->sih, gci_intmask),
				~0, mask | GCI_INTMASK_STFAE);
	}
}

#if defined(SAVERESTORE)
static bool
rte_shif_uart_sr_save(void *arg)
{
	shif_info_t *cd = arg;
	rte_shif_uart_flush(cd);

	return TRUE;
}
#endif /* SAVERESTORE */

static void
rte_shif_uart_proc(void *cbdata)
{
	shif_info_t *cd = (shif_info_t *)cbdata;
	int iir_st, lsr, c;

	if (cd == NULL)
		return;

	/* Can read IIR only once per ISR */
	iir_st = serial_in(cd->uart, UART_IIR);

	/* Ready for next TX? */
	if (iir_st == UART_IIR_THRE) {
		/* Force output of characters until tx_buf is empty */
		hnd_timer_start(cd->gci_uart_tx_timer, 0, TRUE);
	}

	/* Input available? */
	if (!((lsr = serial_in(cd->uart, UART_LSR)) & UART_LSR_RXRDY)) {
		return;
	}

	/* Get the char */
	c = serial_in(cd->uart, UART_RX);

	if (cd->uart_rx_cb)
		cd->uart_rx_cb(cd->uart_rx_cb_ctx, (uint8)c);

}

static bool
rte_shif_uart_worklet(void *cbdata)
{
	rte_shif_uart_proc(cbdata);

	/* Don't reschedule */
	return FALSE;
}
/* gci_serial_puts: Force output of characters until tx_buf is empty */

static void
gci_serial_puts(shif_info_t *cd)
{
	int out_idx = 0;

	for (out_idx = 0; out_idx < cd->tx_count; out_idx++) {
		while (serial_in(cd->uart, UART_LSR) & UART_LSR_THRE)
			;
		serial_out(cd->uart, UART_TX,  cd->tx_buf[out_idx]);
	}

	cd->tx_count -= out_idx;

	if (!cd->tx_count) {
		si_gci_indirect(cd->sih, 0,
			GCI_OFFSETOF(cd->sih, gci_intmask),
			GCI_INTMASK_STFAE, 0);
	}
}

static void
rte_shif_uart_tx_timer_handler(hnd_timer_t *t)
{
	si_t *sih = (si_t *)hnd_timer_get_ctx(t);
	shif_info_t *cd = hnd_timer_get_data(t);

	hnd_timer_stop(cd->gci_uart_tx_timer);
	si_gci_indirect(sih, 0,
		GCI_OFFSETOF(sih, gci_intmask),
		GCI_INTMASK_STFAE, 0);
	if (cd->tx_count != 0) {
		gci_serial_puts(cd);
	}
}

static int
rte_shif_uart_tx(shif_info_t *shif_p, uint8 *buf, int len)
{
	shif_info_t *cd = shif_p;

	if (!buf) {
		SHIF_DBG(("%s: buf is NULL \n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (cd->uart) {
		int mask = 0;

		bcopy(buf, cd->tx_buf, len);
		cd->tx_count = len;

		/* Check TX FIFO Empty status */
		mask = si_gci_direct(cd->sih, GCI_OFFSETOF(cd->sih, gci_intmask), 0, 0);
		si_gci_direct(cd->sih, GCI_OFFSETOF(cd->sih, gci_intmask),
				~0, mask | GCI_INTMASK_STFAE);
	} else {
		hnd_uart_tx(buf, len);
	}

	return BCME_OK;
}

void *
rte_register_shif_rx_cb(void *ctx, uart_rx_cb_t cb)
{
	shif_info_t *cd = (shif_info_t *)ctx;

	if (cd->uart) {
		cd->uart_rx_cb = cb;
		cd->uart_rx_cb_ctx = ctx;
	} else {
		hnd_register_uart_rx_cb(ctx, rte_shif_uart_rx);
	}
	return cd;
}

shif_info_t*
BCMATTACHFN(rte_shif_init)(si_t *sih, osl_t *osh, uint8 sh_pin, uint8 int_pin,
		tx_status_cb_t tx_status_cb)
{
	shif_info_t *shif_p;
	uint8 wake_events;
	uint8 wake_int_pin;

	/* allocate sensor hub interface context */
	shif_p = MALLOCZ(osh, sizeof(shif_info_t));
	if (!shif_p) {
		SHIF_DBG(("%s: No memory!\n", __FUNCTION__));
		return NULL;
	}
	shif_p->sih = sih;
	shif_p->osh = osh;
	shif_p->uart = NULL;

#ifndef	SHIF_DISABLE_WP
	shif_p->wp_p = rte_shif_wp_init(shif_p, sih, osh, sh_pin, int_pin);
#else
	BCM_REFERENCE(rte_shif_wp_init);
#endif	/* SHIF_DISABLE_WP */

	if (sih != NULL && UART_ENAB()) {
		/* bind the gci uart for sensor hub interface communication */
		shif_p->uart = serial_bind_dev(shif_p->sih,
				RTE_SHIF_UART_ID, RTE_UART_TYPE_GCI,
				NULL, rte_shif_uart_worklet, shif_p);

		shif_p->tx_count = 0;
		shif_p->tx_buf = MALLOCZ(osh, SHIF_TX_BUF_SIZE);
		if (!shif_p->gci_uart_tx_timer) {
			shif_p->gci_uart_tx_timer = hnd_timer_create((void *) shif_p->sih, shif_p,
					rte_shif_uart_tx_timer_handler, NULL, NULL);
		}
#if defined(SAVERESTORE)
		sr_register_save(shif_p->sih, rte_shif_uart_sr_save, (void *)shif_p);
#endif // endif
	}
	/* setting a interrupt detect way */

	wake_events = (1 << GCI_GPIO_STS_NEG_EDGE_BIT);
	wake_events |= (1 << GCI_GPIO_STS_VALUE_BIT);
	wake_events |= (1 << GCI_GPIO_STS_POS_EDGE_BIT);

	wake_int_pin = shif_p->wp_p->int_pin;

	si_gci_shif_config_wake_pin(sih, wake_int_pin, wake_events, FALSE);
	shif_p->wp_p->gci_wake_handle = hnd_enable_gci_gpioint(wake_int_pin, wake_events,
			_wp_wake_handler, (void *)shif_p->wp_p);

	shif_p->wp_p->tx_status_cb = tx_status_cb;

	return shif_p;
}

void
BCMATTACHFN(rte_shif_deinit)(shif_info_t *shif_p)
{
	if (!shif_p) {
		return;
	}

	/* unregister console callback */
	rte_register_shif_rx_cb(shif_p, NULL);

	if (shif_p->wp_p) {

		/* unregister wake handler */
		if (shif_p->wp_p->gci_wake_handle) {
			SHIF_INFO(("%s: wake pin handler free\n", __FUNCTION__));
			hnd_disable_gci_gpioint(shif_p->wp_p->gci_wake_handle);
		}

		rte_shif_wp_deinit(shif_p->wp_p);

		shif_p->wp_p = NULL;
	}

	MFREE(shif_p->osh, shif_p->tx_buf, SHIF_TX_BUF_SIZE);
	MFREE(shif_p->osh, shif_p, sizeof(shif_info_t));
	shif_p = NULL;
}

static void
rte_shif_update_txstatus(shif_wp_t *wp_p, uint8 txstatus)
{
	uint16 pkt_type = wp_p->pkt_type;
	/*
	 * If call tx_status_cb prior than wp_p->pkt_type = 0xff;
	 * then wp_p->pkt_typ is not set to 0xff because the next new pkt tx
	 * is set pkt_type. It would be issue pkt_type confused
	 * If tx operation is completed, pkt_type has to be initilized to 0xff
	 * So, It have to be updated wp_p->pkt_type = 0xff firstly
	 */
	wp_p->tx_buf = NULL;
	wp_p->pkt_type = 0xff;
	wp_p->xstate = SHIF_TX_ALLOWED;

	wp_p->tx_status_cb(wp_p->tx_status_cb_param, pkt_type, txstatus);
}

static void
rte_shif_update_rxpkt(shif_wp_t *wp_p)
{
	shif_info_t	*shif_p;
	shif_rx_ctxt_t	*rx_ctx_p;
	uint16		*bytes_rxd;
	void		*user_ctx;

	if (wp_p == NULL || wp_p->shif_p == NULL) {
		SHIF_DBG(("%s: Invalid wp_p instance\n", __FUNCTION__));
		return;
	}

	shif_p = wp_p->shif_p;
	rx_ctx_p = &shif_p->rx_ctxt;
	bytes_rxd = &rx_ctx_p->bytes_rxd;

	if (rx_ctx_p->rx_cb_param == NULL || rx_ctx_p->rx_cb == NULL ||
			rx_ctx_p->buf == NULL) {
		SHIF_DBG(("%s: Invalid rx_ctx_p instance\n", __FUNCTION__));
		return;
	}

	user_ctx = rx_ctx_p->rx_cb_param;

	rx_ctx_p->rx_cb(user_ctx, (shub_pkt_t *)rx_ctx_p->buf, *bytes_rxd);
	*bytes_rxd = 0;
}

void
rte_shif_enable(shif_info_t *shifp, bool enable)
{
	shif_wp_t *wp_p;
	uint8 wake_events;

	if (shifp == NULL || shifp->wp_p == NULL) {
		SHIF_DBG(("%s: Invalid shifp instance\n", __FUNCTION__));
		return;
	}
	wp_p = shifp->wp_p;
	shifp->shif_enable = enable;

	wp_p->state = (enable)? WP_SLEEP : WP_DISABLE;

	wake_events = (1 << GCI_GPIO_STS_NEG_EDGE_BIT);
	wake_events |= (1 << GCI_GPIO_STS_VALUE_BIT);
	wake_events |= (1 << GCI_GPIO_STS_POS_EDGE_BIT);

	si_shif_int_enable(wp_p->sih, wp_p->int_pin, wake_events, enable);

}

/* In 4359 test, chip did not waked by falling edge,
 * So It changed to LOW level interrupt. SensorHub_to_WLAN interrtup is
 * keep LOW signal for recieving UART data. In this time, LOW level
 * interrupt has to be disabled. rte_shif_interrupt_enable() is only for disable
 * LOW level interrupt. It does not disable wake interrupt (rising edge)
 */
static void
rte_shif_interrupt_enable(shif_wp_t *wp_p, bool enable)
{
	uint32 opt;

	opt = GCI_INTSTATUS_GPIOINT;

	si_gci_indirect(wp_p->sih, 0, GCI_OFFSETOF(wp_p->sih,
			gci_intmask), opt, (enable)? opt : 0);
}

#ifdef RTESHIF_DBG
void
reg_dump(shif_wp_t *wp_p)
{
	uint32 reg_pmu, reg_pmu6;
	uint32 reg[11];

	reg_pmu = si_pmu_chipcontrol(wp_p->sih, PMU_CHIPCTL2, 0, 0);
	reg_pmu6 = si_pmu_chipcontrol(wp_p->sih, PMU_CHIPCTL6, 0, 0);
	printf("%s:pmu chip ctrl2	 0x%x \n", __FUNCTION__, reg_pmu);
	printf("%s:pmu chip ctrl6	 0x%x \n", __FUNCTION__, reg_pmu6);
	printf("%s: check wake & Intterrupt setting\n", __FUNCTION__);

	si_corereg(wp_p->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gci_indirect_addr), ~0, 0);

	reg[0] = si_corereg(wp_p->sih, SI_CC_IDX,
			OFFSETOF(chipcregs_t, gci_indirect_addr), 0, 0);
	reg[1] = si_corereg(wp_p->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gci_gpioctl), 0, 0);
	reg[2] = si_corereg(wp_p->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gci_chipctrl), 0, 0);
	reg[3] = si_corereg(wp_p->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gci_chipsts), 0, 0);
	reg[4] = si_corereg(wp_p->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gci_gpiointmask), 0, 0);
	reg[5] = si_corereg(wp_p->sih, SI_CC_IDX,
			OFFSETOF(chipcregs_t, gci_gpiowakemask), 0, 0);
	reg[6] = si_corereg(wp_p->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gci_intmask), 0, 0);
	reg[7] = si_corereg(wp_p->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gci_wakemask), 0, 0);
	reg[8] = si_corereg(wp_p->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gci_gpiostatus), 0, 0);
	reg[9] = si_corereg(wp_p->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, intstatus), 0, 0);
	reg[10] = si_corereg(wp_p->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, intmask), 0, 0);

	printf("0xc40 gci_indirect_addr : 0x%8x\n", reg[0]);
	printf("0xc44 gci_gpioctl : 0x%8x\n", reg[1]);
	printf("0xe00 gci_chipctrl : 0x%8x\n", reg[2]);
	printf("0xe04 gci_chipsts : 0x%8x\n", reg[3]);
	printf("0xc58 gci_gpiointmask : 0x%8x\n", reg[4]);
	printf("0xc5c gci_gpiowakemask : 0x%8x\n", reg[5]);
	printf("0xc18 gci_intmask : 0x%8x\n", reg[6]);
	printf("0xc1c gci_wakemask : 0x%8x\n", reg[7]);
	printf("0xc48 gci_gpiostatus : 0x%8x\n", reg[8]);
	printf("0x20 intstatus : 0x%8x\n", reg[9]);
	printf("0x24 intmask : 0x%8x\n", reg[10]);

}
#endif /* RTESHIF_DBG */

static void
_wp_event_sleep(shif_wp_t *wp_p, uint8 evt)
{
	si_t	*sih = wp_p->sih;

	if (evt == WP_EVT_TX) {
		SHIF_DBG(("%s %s event in %s state \n", __FUNCTION__,
				event_strings[evt], state_strings[wp_p->state]));
		/* keep the chip awake */
		wp_p->clk_sts_reg = si_corereg(sih, SI_CC_IDX,
				OFFSETOF(chipcregs_t, clk_ctl_st), 0, 0);
		si_corereg(sih, SI_CC_IDX,
				OFFSETOF(chipcregs_t, clk_ctl_st),
				CCS_FORCEALP, CCS_FORCEALP);

		/* tx wake request */
		if (!wp_p->shif_4_wire) {
			rte_shif_uart_tx(wp_p->shif_p, &wp_p->wake_req, 1);
		} else {
			si_gpioout(sih, 1 << wp_p->sh_pin,
					0, GPIO_HI_PRIORITY);
		}
		wp_p->state = WP_IDLE;
		wp_p->xstate = SHIF_TX_WAITING;

		/* start timer */
		hnd_timer_start(wp_p->timer, wp_p->timeout, FALSE);
	} else if (evt == WP_EVT_LOW) {
		SHIF_DBG(("%s %s event in %s state \n", __FUNCTION__,
				event_strings[evt], state_strings[wp_p->state]));
		if (wp_p->xstate == SHIF_TX_ALLOWED) {
			wp_p->clk_sts_reg = si_corereg(sih, SI_CC_IDX,
					OFFSETOF(chipcregs_t, clk_ctl_st), 0, 0);
			si_corereg(sih, SI_CC_IDX,
					OFFSETOF(chipcregs_t, clk_ctl_st),
					CCS_FORCEALP, CCS_FORCEALP);

			si_gpioout(sih, 1 << wp_p->sh_pin,
					0, GPIO_HI_PRIORITY);

			wp_p->state = WP_ACTIVE;
			wp_p->xstate = SHIF_RX_WAITING;

			/* start timer */
			hnd_timer_start(wp_p->timer, wp_p->timeout, FALSE);
		}
		else {
			SHIF_DBG(("%s: blocking interrupt on pin operating\n",
				__FUNCTION__));
			if (pinblock_cnt >= MAX_PINBLOCK) {
				pinblock_cnt = 0;
				wp_p->xstate = SHIF_TX_FAILED;
				SHIF_DBG(("%s: unlock blocking state\n", __FUNCTION__));
			} else {
				pinblock_cnt++;
			}
		}
	}
}

static void
_wp_event_idle(shif_wp_t *wp_p, uint8 evt)
{
	si_t	*sih = wp_p->sih;

	if (evt == WP_EVT_LOW) {
		SHIF_DBG(("%s %s event in %s state \n", __FUNCTION__,
				event_strings[evt], state_strings[wp_p->state]));
		wp_p->state = WP_ACTIVE;

		/* stop timer */
		hnd_timer_stop(wp_p->timer);

		_wp_send_data(wp_p);

		/* start timer */
		hnd_timer_start(wp_p->timer, wp_p->timeout, FALSE);
	} else if (evt == WP_EVT_TX) {
		SHIF_DBG(("%s %s event in %s state \n", __FUNCTION__,
				event_strings[evt], state_strings[wp_p->state]));
		if (wp_p->xstate == SHIF_RX_WAITING) {
			SHIF_DBG(("%s: wating for WLAN_WAKE pin HIGH signal\n",
				__FUNCTION__));
		} else {
			SHIF_DBG(("%s: wrong tx time - WLAN_WAKE pin is not LOW\n",
				__FUNCTION__));
		}
	} else if (evt == WP_EVT_TO || evt == WP_EVT_HIGH) {
		SHIF_DBG(("%s %s event in %s state \n", __FUNCTION__,
				event_strings[evt], state_strings[wp_p->state]));
		si_gpioout(sih, 1 << wp_p->sh_pin,
				1 << wp_p->sh_pin, GPIO_HI_PRIORITY);
		wp_p->state = WP_SLEEP;

		/* stop timer */
		hnd_timer_stop(wp_p->timer);

		/* allow the chip to sleep */
		si_corereg(sih, SI_CC_IDX,
				OFFSETOF(chipcregs_t, clk_ctl_st),
				CCS_FORCEALP, 0);
		wp_p->clk_sts_reg = si_corereg(sih, SI_CC_IDX,
				OFFSETOF(chipcregs_t, clk_ctl_st), 0, 0);
		if (evt == WP_EVT_TO) {
			wp_p->xstate = SHIF_TX_FAILED;
			SHIF_DBG(("%s:IDLE state :WLAN_WAKE signal wait timeout\n",
				__FUNCTION__));
		} else {
			SHIF_DBG(("%s:IDLE state :WLAN_WAKE HIGH. RX complete\n",
				__FUNCTION__));
		}
		if (wp_p->xstate == SHIF_RX_COMPLETE) {
			wp_p->xstate = SHIF_TX_ALLOWED;
			rte_shif_update_rxpkt(wp_p);
		}
	}
}

static void
_wp_event_active(shif_wp_t *wp_p, uint8 evt)
{
	si_t	*sih = wp_p->sih;

	/* stop timer */
	hnd_timer_stop(wp_p->timer);
	if (evt == WP_EVT_HIGH || evt == WP_EVT_TO) {
		SHIF_DBG(("%s %s event in %s state pkt_type %d\n", __FUNCTION__,
				event_strings[evt], state_strings[wp_p->state],
				wp_p->pkt_type));
		si_gpioout(sih, 1 << wp_p->sh_pin,
				1 << wp_p->sh_pin, GPIO_HI_PRIORITY);
		wp_p->state = WP_SLEEP;

		/* allow the chip to sleep */
		si_corereg(sih, SI_CC_IDX,
				OFFSETOF(chipcregs_t, clk_ctl_st),
				CCS_FORCEALP, 0);
		wp_p->clk_sts_reg = si_corereg(sih, SI_CC_IDX,
				OFFSETOF(chipcregs_t, clk_ctl_st), 0, 0);

		if (evt == WP_EVT_HIGH) {
			wp_p->xstate = SHIF_TX_COMPLETE;
		} else if (evt == WP_EVT_TO) {
			if (wp_p->xstate == SHIF_TX_WAITING) {
				wp_p->xstate = SHIF_TX_FAILED;
				SHIF_DBG(("%s:ACTIVE state: TX failed by "
							"WLAN_WAKE HIGH timeout\n", __FUNCTION__));
			} else if (wp_p->xstate == SHIF_RX_WAITING) {
				/*
				 * If do tx_allowed = SHIF_RX_FAILED,
				 * the next signal doesn't recv
				 * so It have to set SHIF_TX_ALLOWED
				 */
				wp_p->xstate = SHIF_TX_ALLOWED;
				SHIF_DBG(("%s:ACTIVE state: RX failed by RX data "
							"wait timeout\n", __FUNCTION__));
			} else {
				/*
				 * If do tx_allowed = SHIF_RX_FAILED,
				 * the next signal doesn't recv
				 * so It have to set SHIF_TX_ALLOWED
				 */
				SHIF_DBG(("%s:ACTIVE state: wait timeout\n",
					__FUNCTION__));
				wp_p->xstate = SHIF_TX_ALLOWED;
			}
		}
	}
	else if (evt == WP_EVT_RX_CPLT) {
		wp_p->xstate = SHIF_RX_COMPLETE;
		SHIF_DBG(("%s %s event in %s state \n", __FUNCTION__,
				event_strings[evt], state_strings[wp_p->state]));
		si_gpioout(sih, 1 << wp_p->sh_pin,
				1 << wp_p->sh_pin, GPIO_HI_PRIORITY);
		wp_p->state = WP_IDLE;
		hnd_timer_start(wp_p->timer, wp_p->timeout, FALSE);
	} else {
		/* start timer */
		hnd_timer_start(wp_p->timer, wp_p->timeout, FALSE);
	}
}
