/*
 * Broadcom LACP fsm module
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
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
 * $Id$
 */

#ifndef _LACP_FSM_H_
#define _LACP_FSM_H_

#include <osl.h>
#include <proto/bcmlacp.h>

/* port_id */
typedef enum {
	LACP_PORT_0,
	LACP_PORT_1,
	LACP_PORT_2,
	LACP_PORT_3,
	LACP_PORT_4,
	LACP_PORT_MAX
} port_id_t;
#define LACP_ALL_PORTS_BITMASK		0x1E

#define MAX_SWAGGR_PORTS		16
#define MAX_PHYAGGR_PORTS		2
#define MAX_LAG_PORTS			LACP_PORT_MAX



/* Port state definitions
 * (43.4.2.2 in the 802.3ad standard)
 */
#define FSM_STATE_LACP_ACTIVITY   0x01
#define FSM_STATE_LACP_TIMEOUT    0x02
#define FSM_STATE_AGGREGATION     0x04
#define FSM_STATE_SYNCHRONIZATION 0x08
#define FSM_STATE_COLLECTING      0x10
#define FSM_STATE_DISTRIBUTING    0x20
#define FSM_STATE_DEFAULTED       0x40
#define FSM_STATE_EXPIRED         0x80


/* Recceive machine states
 * (43.4.12 in the 802.3ad standard)
 */
typedef enum {
	FSM_RX_INITIALIZE,
	FSM_RX_PORT_DISABLED,
	FSM_RX_LACP_DISABLED,
	FSM_RX_EXPIRED,
	FSM_RX_DEFAULTED,
	FSM_RX_CURRENT
} fsm_rx_states_t;

/* Periodic machine states
 * (43.4.13 in the 802.3ad standard)
 */
typedef enum {
	FSM_NO_PERIODIC,
	FSM_FAST_PERIODIC,
	FSM_SLOW_PERIODIC,
	FSM_PERIODIC_TX
} fsm_periodic_states_t;

/* Mux machine states
 * (43.4.15 in the 802.3ad standard) - coupled countrol
 */
typedef enum {
	FSM_MUX_DETACHED,
	FSM_MUX_WAITING,
	FSM_MUX_ATTACHED,
	FSM_MUX_COLLECTING_DISTRIBUTING
} fsm_mux_states_t;


/* rx indication types */
typedef enum {
	FSM_SUBTYPE_LACPDU = 1,	/* subtype lacpdu */
	FSM_SUBTYPE_MARKER	/* subtype marker */
} fsm_pdu_type_t;


struct port_params {
	uint8 system[ETHER_ADDR_LEN];
	uint16 system_priority;
	uint16 key;
	uint16 port_number;
	uint16 port_priority;
	uint16 port_state;
};

/* port structure, variables associated with each port.
 * (43.4.7 in the 802.3ad standard)
 */
typedef struct port {
	uint16 actor_port_number;
	uint16 actor_port_priority;
	uint16 actor_port_aggregator_identifier;
	bool ntt;
	uint16 actor_oper_port_key;
	uint8 actor_admin_port_state;
	uint8 actor_oper_port_state;
	struct port_params partner_admin;
	struct port_params partner_oper;
	bool is_linkup;
	bool is_fulldplx;
	uint32 speed;
	bool is_timer_started;
	bool rcvd_lacp;

	/* Following parameter is not specified in the standard, just for simplification */
	uint8 actor_system[ETHER_ADDR_LEN];
	uint16 actor_system_priority;

	/* ===== PRIVATE PARAMETERS ===== */
	/* To avoid race condition between callback and receive interrupt */
	spinlock_t rx_machine_lock;

	uint16 sm_vars;				/* all state machines variables for this port */
	fsm_rx_states_t sm_rx_state;		/* state machine rx state */
	fsm_periodic_states_t sm_periodic_state; /* state machine periodic state */
	fsm_mux_states_t sm_mux_state;		/* state machine mux state */
	bool rx_sm_expired;
	uint8 cur_periodic_timeout;

	struct lacpdu lacpdu;			/* the lacpdu that will be sent for this port */
	bool is_agg;
	uint8 lagid;
	struct aggregator *aggr;
	bool is_phy_agg;
} fsm_port_t;


/* aggregator structure, variables asspociated with each Aggregation.
 * (43.4.6 in the 802.3ad standard)
 */
typedef struct aggregator {
	uint16 aggregator_identifier;
	uint16 actor_admin_aggregator_key;
	uint16 actor_oper_aggregator_key;
	uint8 partner_system[ETHER_ADDR_LEN];
	uint16 partner_system_priority;
	uint16 partner_oper_aggregator_key;
	uint16 receive_state;
	uint16 transmit_state;
	struct port *lag_ports[MAX_LAG_PORTS];

	bool is_active;
	uint16 num_of_ports;
	uint16 ports_mask;
} fsm_aggregator_t;


typedef struct fsm_info {
	struct port port_info[MAX_LAG_PORTS];
	struct aggregator sw_aggr[MAX_SWAGGR_PORTS];
	struct aggregator phy_aggr[MAX_PHYAGGR_PORTS];
	char hostmac[ETHER_ADDR_LEN];

	osl_t *osh;
	void *lacpc;
	bool lacp_active_mode;
	uint16 ports_mask;
	uint8 lastport;
} fsm_info_t;


typedef struct fsm_status {
	bool lacp_active_mode;
	bool lag_actived[MAX_SWAGGR_PORTS];
	uint16 lag_ports_mask[MAX_SWAGGR_PORTS];
	bool port_is_agg[MAX_LAG_PORTS];
	uint8 port_lag[MAX_LAG_PORTS];
	int8 port_actor_state[MAX_LAG_PORTS];
	int8 port_partner_state[MAX_LAG_PORTS];
} fsm_status_t;

typedef struct fsm_lag_status {
	struct aggregator sw_aggr[MAX_SWAGGR_PORTS];
	struct aggregator phy_aggr[MAX_PHYAGGR_PORTS];
} fsm_lag_status_t;

void *fsm_init(void *lacpc, osl_t *osh);
int fsm_deinit(void *ctx);
void fsm_get_lacp_status(void* ctx, struct fsm_status* fsm_status);
void fsm_get_lacp_lag_status(void* ctx, struct fsm_lag_status* fsm_lag_status);

#endif /* _LACP_FSM_H_ */
