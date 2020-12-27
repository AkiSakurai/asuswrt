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

#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14)
#include <linux/module.h>
#endif

#include <linux/types.h>
#include <linux/errno.h>
#include <epivers.h>

#include "lacp_fsm.h"
#include "lacp_timer.h"
#include "lacpc_export.h"
#include "lacpc.h"
#include "lacp_proc.h"
#include "lacp_debug.h"

static const char *dbg_rx_state[] = { "FSM_RX_INITIALIZE", "FSM_RX_PORT_DISABLED",
					"FSM_RX_LACP_DISABLED", "FSM_RX_EXPIRED",
					"FSM_RX_DEFAULTED", "FSM_RX_CURRENT", 0 };

static const char *dbg_mux_state[] = { "FSM_MUX_DETACHED", "FSM_MUX_WAITING",
					"FSM_MUX_ATTACHED", "FSM_MUX_COLLECTING_DISTRIBUTING", 0 };

					/* Port Key definitions
 * key is determined according to the link speed, duplex and
 * user key(which is yet not supported)
 *              ------------------------------------------------------------
 * Port key :   | User key                       |      Speed       |Duplex|
 *              ------------------------------------------------------------
 *              16                               6                  1      0
 */
#define  FSM_DUPLEX_KEY_BITS    0x1
#define  FSM_SPEED_KEY_BITS     0x3E
#define  FSM_USER_KEY_BITS      0xFFC0
#define  FSM_USER_KEY_INDEX     6

/* compare MAC addresses */
#define MAC_ADDRESS_COMPARE(A, B) memcmp(A, B, ETHER_ADDR_LEN)
#define MAC_ADDRESS_COPY(A, B) memcpy(A, B, ETHER_ADDR_LEN)
static uint8 NULL_MAC_ADDR[ETHER_ADDR_LEN] = {0, 0, 0, 0, 0, 0};


/* Timer definitions
 * (43.4.4 in the 802.3ad standard)
 */
#define FSM_FAST_PERIODIC_TIME      1
#define FSM_SLOW_PERIODIC_TIME      30
#define FSM_SHORT_TIMEOUT_TIME      (3*FSM_FAST_PERIODIC_TIME)
#define FSM_LONG_TIMEOUT_TIME       (3*FSM_SLOW_PERIODIC_TIME)
#define FSM_CHURN_DETECTION_TIME    60
#define FSM_AGGREGATE_WAIT_TIME     2

#define FSM_DEFAULT_TIMEOUT		1

/* Port Variables definitions used for mananing the opweation of the State Machines
 * (43.4.8 in the 802.3ad standard)
 */
#define FSM_PORT_BEGIN           0x0001
#define FSM_PORT_LACP_ENABLED    0x0002
#define FSM_PORT_ACTOR_CHURN     0x0004
#define FSM_PORT_PARTNER_CHURN   0x0008
#define FSM_PORT_READY_N         0x0010
#define FSM_PORT_READY           0x0020
#define FSM_PORT_SELECTED        0x0040
#define FSM_PORT_MOVED           0x0080
#define FSM_PORT_MATCHED         0x0100

#define RXFSM_LOCK_INIT(port) \
	do {if (port) spin_lock_init(&(port->rx_machine_lock));} while (0)
#define RXFSM_LOCK(port) \
	do {if (port) spin_lock_bh(&(port->rx_machine_lock));} while (0)
#define RXFSM_UNLOCK(port) \
	do {if (port) spin_unlock_bh(&(port->rx_machine_lock));} while (0)

#define FSM_SET_STATE(state, value) \
	do {if (state) *state |= value;} while (0)
#define FSM_CLEAR_STATE(state, value) \
	do {if (state) *state &= ~value;} while (0)

/* rx indication types */
typedef enum {
	PDU_TYPE_LACPDU = 1,	/* type lacpdu */
	PDU_TYPE_MARKER		/* type marker */
} pdu_type_t;

static bool initialized = FALSE;
static void fsm_clear_agg(struct aggregator *aggr, int aggr_id);


static void
_start_timer(struct port *port, int8 portid)
{
	/* start lacp rx machine timer */
	lacp_timer_start(portid, LACP_TIMER_RX_MACHINE, FSM_DEFAULT_TIMEOUT, TRUE);
	/* start lacp periodic tx machine timer */
	lacp_timer_start(portid, LACP_TIMER_PERIODIC_TX, FSM_DEFAULT_TIMEOUT, TRUE);
	port->cur_periodic_timeout = FSM_SHORT_TIMEOUT_TIME;
	/* start lacp selection logic timer */
	lacp_timer_start(portid, LACP_TIMER_SELECT_LOGIC, FSM_DEFAULT_TIMEOUT, TRUE);
	/* start lacp mux machine timer */
	lacp_timer_start(portid, LACP_TIMER_MUX_MACHINE, FSM_DEFAULT_TIMEOUT, TRUE);
	/* start lacp tx machine timer */
	lacp_timer_start(portid, LACP_TIMER_TX_MACHINE, FSM_DEFAULT_TIMEOUT, TRUE);

	port->is_timer_started = TRUE;
}


static void
_stop_timer(struct port *port, int8 portid)
{
	/* stop lacp rx machine timer */
	lacp_timer_stop(portid, LACP_TIMER_RX_MACHINE);
	/* stop lacp periodic tx machine timer */
	lacp_timer_stop(portid, LACP_TIMER_PERIODIC_TX);
	port->cur_periodic_timeout = 0;
	/* stop lacp selection logic timer */
	lacp_timer_stop(portid, LACP_TIMER_SELECT_LOGIC);
	/* stop lacp mux machine timer */
	lacp_timer_stop(portid, LACP_TIMER_MUX_MACHINE);
	/* stop lacp tx machine timer */
	lacp_timer_stop(portid, LACP_TIMER_TX_MACHINE);

	port->is_timer_started = FALSE;
}


/**
 * _update_lacpdu_from_port - update a port's lacpdu fields
 * @port: the port we're looking at
 *
 */
static void
_update_lacpdu_from_port(struct port *port)
{
	struct lacpdu *lacpdu = &port->lacpdu;
	const struct port_params *partner = &port->partner_oper;

	/* update current actual Actor parameters */
	lacpdu->actor_system_priority = htons(port->actor_system_priority);
	MAC_ADDRESS_COPY(lacpdu->actor_system, port->actor_system);
	lacpdu->actor_key = htons(port->actor_oper_port_key);
	lacpdu->actor_port_priority = htons(port->actor_port_priority);
	lacpdu->actor_port = htons(port->actor_port_number);
	lacpdu->actor_state = port->actor_oper_port_state;
	lacpdu->partner_system_priority = htons(partner->system_priority);
	MAC_ADDRESS_COPY(lacpdu->partner_system, partner->system);
	lacpdu->partner_key = htons(partner->key);
	lacpdu->partner_port_priority = htons(partner->port_priority);
	lacpdu->partner_port = htons(partner->port_number);
	lacpdu->partner_state = partner->port_state;
}


/*
 * _is_aggr_ready - check if all ports in an aggregator are ready
 */
static bool
_is_aggr_ready(struct aggregator *aggr)
{
	int i;
	bool retval = TRUE;
	struct port *port;

	if (!aggr)
		return FALSE;

	/* scan all ports in this aggregator to verfy if they are all ready */
	for (i = 0; i < MAX_LAG_PORTS; i++) {
		if (aggr->ports_mask & (1 << i)) {
			port = aggr->lag_ports[i];
			ASSERT(port);
			if (!(port->sm_vars & FSM_PORT_READY_N)) {
				retval = FALSE;
				break;
			}
		}
	}
	return retval;
}


/*
 * _set_aggr_ports_ready - set value of Ready bit in all ports of an aggregator
 */
static void
_set_aggr_ports_ready(struct aggregator *aggr, struct port *port, int val)
{
	if (!aggr || !port)
		return;

	LACP_SELECT(("%s: aggr->ports_mask 0x%x, port->actor_port_number %d, val %d\n",
		__func__, aggr->ports_mask, port->actor_port_number, val));
	if (aggr->ports_mask & (1 << port->actor_port_number)) {
		if (val)
			FSM_SET_STATE(&(port->sm_vars), FSM_PORT_READY);
		else
			FSM_CLEAR_STATE(&(port->sm_vars), FSM_PORT_READY);
	}
	return;
}

/*
 * _check_port_canbe_select -  is port match this agg
 */
static bool
_check_port_canbe_select(struct port *port, int portid)
{
	if ((port->sm_vars & FSM_PORT_SELECTED) ||
		(!port->rcvd_lacp) ||
		(!port->is_fulldplx) ||
		(!MAC_ADDRESS_COMPARE(&(port->partner_oper.system), &NULL_MAC_ADDR))) {

		LACP_SELECT(("_check_port_canbe_select: PORT %d FALSE\n", portid));
		return FALSE;
	}

	LACP_SELECT(("_check_port_canbe_select: PORT %d TRUE\n", portid));
	LACP_SELECT(("port_waiting_select: PORT %d FALSE\n", portid));
	LACP_SELECT(("PORT %d FSM_PORT_SELECTED: %s\n", portid,
			(port->sm_vars & FSM_PORT_SELECTED) ? "TRUE" : "FALSE"));
	LACP_SELECT(("PORT %d (!port->rcvd_lacp): %s\n", portid,
			(!port->rcvd_lacp) ? "TRUE" : "FALSE"));
	LACP_SELECT(("PORT %d (!port->is_fulldplx): %s\n", portid,
			(!port->is_fulldplx) ? "TRUE" : "FALSE"));
	LACP_SELECT(("PORT %d (NULL_MAC_ADDR): %s\n", portid,
			(!MAC_ADDRESS_COMPARE(&(port->partner_oper.system), &NULL_MAC_ADDR)) ? "TRUE" : "FALSE"));

	return TRUE;
}

/*
 * _check_port_agg_match -  is port match this agg
 */
static bool
_check_port_agg_match(struct aggregator *aggr, struct port *port)
{
	bool match = FALSE;

	if (!MAC_ADDRESS_COMPARE(&(aggr->partner_system), &(port->partner_oper.system)) &&
		(aggr->partner_system_priority == port->partner_oper.system_priority) &&
		(aggr->partner_oper_aggregator_key == port->partner_oper.key)) {

		match = TRUE;
	}
	return match;
}

static void
_print_lacp_state(struct port *port)
{
	if (lacp_msg_level & LACP_FSM_VAL) {
		printf("LACP Actor   state: ");
		if (port->lacpdu.actor_state & FSM_STATE_LACP_ACTIVITY)
			printf("\"LACP Activity\" ");
		if (port->lacpdu.actor_state & FSM_STATE_LACP_TIMEOUT)
			printf("\"LACP Timeout\" ");
		if (port->lacpdu.actor_state & FSM_STATE_AGGREGATION)
			printf("\"Aggregation\" ");
		if (port->lacpdu.actor_state & FSM_STATE_SYNCHRONIZATION)
			printf("\"Synchronization\" ");
		if (port->lacpdu.actor_state & FSM_STATE_COLLECTING)
			printf("\"Collocting\" ");
		if (port->lacpdu.actor_state & FSM_STATE_DISTRIBUTING)
			printf("\"Distributing\" ");
		if (port->lacpdu.actor_state & FSM_STATE_DEFAULTED)
			printf("\"Defaulted\" ");
		if (port->lacpdu.actor_state & FSM_STATE_EXPIRED)
			printf("\"Expired\" ");
		printf("\n");

		printf("LACP Partner state: ");
		if (port->lacpdu.partner_state & FSM_STATE_LACP_ACTIVITY)
			printf("\"LACP Activity\" ");
		if (port->lacpdu.partner_state & FSM_STATE_LACP_TIMEOUT)
			printf("\"LACP Timeout\" ");
		if (port->lacpdu.partner_state & FSM_STATE_AGGREGATION)
			printf("\"Aggregation\" ");
		if (port->lacpdu.partner_state & FSM_STATE_SYNCHRONIZATION)
			printf("\"Synchronization\" ");
		if (port->lacpdu.partner_state & FSM_STATE_COLLECTING)
			printf("\"Collocting\" ");
		if (port->lacpdu.partner_state & FSM_STATE_DISTRIBUTING)
			printf("\"Distributing\" ");
		if (port->lacpdu.partner_state & FSM_STATE_DEFAULTED)
			printf("\"Defaulted\" ");
		if (port->lacpdu.partner_state & FSM_STATE_EXPIRED)
			printf("\"Expired\" ");
		printf("\n");
	}
}

static void
_print_ports_partner(struct port *port_a, struct port *port_b)
{
	if (lacp_msg_level & LACP_SELECT_VAL) {
		printf("      ========================================\n");
		printf("      portA partner_oper.system %pM\n", port_a->partner_oper.system);
		printf("      portA partner_oper.system_priority %d\n",
			port_a->partner_oper.system_priority);
		printf("      portA partner_oper.key %d\n", port_a->partner_oper.key);
		printf("      portA partner_oper.port_number %d\n",
			port_a->partner_oper.port_number);
		printf("      portA partner_oper.port_priority %d\n",
			port_a->partner_oper.port_priority);
		printf("      ========================================\n");
		printf("      portB partner_oper.system %pM\n", port_b->partner_oper.system);
		printf("      portB partner_oper.system_priority %d\n",
			port_b->partner_oper.system_priority);
		printf("      portB partner_oper.key %d\n", port_b->partner_oper.key);
		printf("      portB partner_oper.port_number %d\n",
			port_b->partner_oper.port_number);
		printf("      portB partner_oper.port_priority %d\n",
			port_b->partner_oper.port_priority);
		printf("      ========================================\n");
	}
}

static void
_print_agg(struct aggregator *aggr)
{
	int i;

	if (lacp_msg_level & LACP_SELECT_VAL) {
		for (i = 0; i < MAX_SWAGGR_PORTS; i++, aggr++) {
			printf("      SW_LAG %d: %s, partner_system %pM, "
				"partner_system_priority %d, "
				"partner_oper_aggregator_key %d\n", i,
				aggr->is_active ? "TRUE " : "FALSE",
				aggr->partner_system,
				aggr->partner_system_priority,
				aggr->partner_oper_aggregator_key);
		}
	}
	return;
}

static void
_dump_lacp_pkt(char* dir, int8 portid, void* pkt, int32 pkt_len)
{
	int i;
	char *pdata = (char*)pkt;

	printf("Dump Port %d %s:\n", portid, dir);
	for (i=0; i<pkt_len; i++) {
		printf("%2.2x ", *(pdata + i));
		if ((i != 0) && (((i + 1) % 16) == 0))
			printf("\n");
	}
	printf("\n");
}

/*
 * Functions
 */
/*
 * _choose_matched - update a port's matched variable from a received lacpdu
 *
 * The FSM_PORT_MATCHED "variable" is not specified by 802.3ad; it is
 * used here to implement the 802.3ad 43.4.9 that requires recordPDU
 * to "match" the LACPDU parameters to the stored values.
 */
static void
_fsm_choose_matched(struct lacpdu *lacpdu, struct port *port)
{
	/* check if all parameters are alike */
	if (((ntohs(lacpdu->partner_port) == port->actor_port_number) &&
	     (ntohs(lacpdu->partner_port_priority) == port->actor_port_priority) &&
	     !MAC_ADDRESS_COMPARE(&(lacpdu->partner_system), &(port->actor_system)) &&
	     (ntohs(lacpdu->partner_system_priority) == port->actor_system_priority) &&
	     (ntohs(lacpdu->partner_key) == port->actor_oper_port_key) &&
	     ((lacpdu->partner_state & FSM_STATE_AGGREGATION) ==
		(port->actor_oper_port_state & FSM_STATE_AGGREGATION))) ||
	    ((lacpdu->actor_state & FSM_STATE_AGGREGATION) == 0)) {
		/* update the state machine Matched variable */
		FSM_SET_STATE(&(port->sm_vars), FSM_PORT_MATCHED);
	} else {
		FSM_CLEAR_STATE(&(port->sm_vars), FSM_PORT_MATCHED);
	}
}


/*
 * _fsm_record_pdu: record parameters from a received lacpdu
 *
 * This funcion records the parameter values for the Actor
 * carried in a received LACPDU as the current Partner operational
 * parameter values, and sets actor_oper_port_state.defaulted to FALSE.
 */
static void
_fsm_record_pdu(struct lacpdu *lacpdu, struct port *port)
{
	struct port_params *partner = &port->partner_oper;

	_fsm_choose_matched(lacpdu, port);
	/* record the new parameter values for the partner operational */
	partner->port_number = ntohs(lacpdu->actor_port);
	partner->port_priority = ntohs(lacpdu->actor_port_priority);
	MAC_ADDRESS_COPY(partner->system, lacpdu->actor_system);
	partner->system_priority = ntohs(lacpdu->actor_system_priority);
	partner->key = ntohs(lacpdu->actor_key);
	partner->port_state = lacpdu->actor_state;

	/* set actor_oper_port_state.defaulted to FALSE */
	port->actor_oper_port_state &= ~FSM_STATE_DEFAULTED;

	/* set the partner sync. to on if the partner is sync. and the port is matched */
	if ((port->sm_vars & FSM_PORT_MATCHED) &&
		(lacpdu->actor_state & FSM_STATE_SYNCHRONIZATION))
		FSM_SET_STATE(&(partner->port_state), FSM_STATE_SYNCHRONIZATION);
	else
		FSM_CLEAR_STATE(&(partner->port_state), FSM_STATE_SYNCHRONIZATION);

	return;
}

/*
 * _fsm_record_default: record default parameters
 *
 * This function records the default parameter values for the Partner
 * carried in the Partner Admin parameters as the current partner operational
 * parameter values and sets actor_oper_port_state.defaulted to TRUE.
 */
static void
_fsm_record_default(struct port *port)
{
	ASSERT(port);
	/* record the partner admin parameters */
	memcpy(&port->partner_oper, &port->partner_admin,
	       sizeof(struct port_params));
	/* set actor_oper_port_state.defaulted to true */
	FSM_SET_STATE(&(port->actor_oper_port_state), FSM_STATE_DEFAULTED);
	return;
}


/*
 * _fsm_update_selected: update a port's Selected variable from a received lacpdu
 *
 * This function updates the value of the Selected variable, using parameter
 * values from a newly received LACPDU. The parameter values for the Actor carried
 * in the received PDU are compared with the corresponding operational parameter
 * values for the ports Partner. If one or more of the comparisons show that
 * the value(s) received in the PDU differ from the current operational values,
 * then selected is set to UNSELECTED. Otherwise, Selected remains unchanged.
 */
static void
_fsm_update_selected(struct lacpdu *lacpdu, struct port *port)
{
	const struct port_params *partner = &port->partner_oper;

	ASSERT(lacpdu);
	ASSERT(port);

	/* check if any parameter is different */
	if (ntohs(lacpdu->actor_port) != partner->port_number ||
	    ntohs(lacpdu->actor_port_priority) != partner->port_priority ||
	    MAC_ADDRESS_COMPARE(&lacpdu->actor_system, &partner->system) ||
	    ntohs(lacpdu->actor_system_priority) != partner->system_priority ||
	    ntohs(lacpdu->actor_key) != partner->key ||
	    (lacpdu->actor_state & FSM_STATE_AGGREGATION) !=
		(partner->port_state & FSM_STATE_AGGREGATION)) {

		LACP_FSM(("Port %d disable SELECT\n", port->actor_port_number));
		/* update the state machine Selected variable */
		FSM_CLEAR_STATE(&(port->sm_vars), FSM_PORT_SELECTED);
	}
	return;
}

/*
 * _fsm_update_default_selected: update a port's Selected variable from Partner
 *
 * This function updates the value of the Selected variable, using the Partner
 * administrative parameter values. The administrative values are compared with
 * the corresponding operational parameter values for the Partner. If one or
 * more of the comparisons shows that the administrative value(s) differ from
 * the current operational values, then Selected is set to UNSELECTED.
 * Otherwise, Selected remains unchanged.
 */
static void
_fsm_update_default_selected(struct port *port)
{
	const struct port_params *admin = &port->partner_admin;
	const struct port_params *oper = &port->partner_oper;

	ASSERT(port);

	/* check if any parameter is different */
	if (admin->port_number != oper->port_number ||
	    admin->port_priority != oper->port_priority ||
	    MAC_ADDRESS_COMPARE(&admin->system, &oper->system) ||
	    admin->system_priority != oper->system_priority ||
	    admin->key != oper->key ||
	    (admin->port_state & FSM_STATE_AGGREGATION)
		!= (oper->port_state & FSM_STATE_AGGREGATION)) {

		LACP_FSM(("Port %d disable SELECT\n", port->actor_port_number));
		/* update the state machine Selected variable */
		FSM_CLEAR_STATE(&(port->sm_vars), FSM_PORT_SELECTED);
	}
	return;
}


/*
 * _fsm_update_ntt: update a port's NTT(Need To Transmit) variable from a
 *                  received lacpdu.
 *
 * This function updates the value of the NTT variable, using parameter values
 * from a newly received LACPDU. The parameter values for the Partner carried
 * in the received PDU are compared with the corresponding operational parameter
 * values for the Actor. If one or more of the comparisons show that the value(s)
 * received in the PDU differ from the current operational values, then NTT is
 * set to TRUE. Otherwise, NTT remains unchanged.
 */
static void
_fsm_update_ntt(struct lacpdu *lacpdu, struct port *port)
{
	/* check if any parameter is different */
	if ((ntohs(lacpdu->partner_port) != port->actor_port_number) ||
	    (ntohs(lacpdu->partner_port_priority) != port->actor_port_priority) ||
	    MAC_ADDRESS_COMPARE(&(lacpdu->partner_system), &(port->actor_system)) ||
	    (ntohs(lacpdu->partner_system_priority) != port->actor_system_priority) ||
	    (ntohs(lacpdu->partner_key) != port->actor_oper_port_key) ||
	    ((lacpdu->partner_state & FSM_STATE_LACP_ACTIVITY) !=
		(port->actor_oper_port_state & FSM_STATE_LACP_ACTIVITY)) ||
	    ((lacpdu->partner_state & FSM_STATE_LACP_TIMEOUT) !=
		(port->actor_oper_port_state & FSM_STATE_LACP_TIMEOUT)) ||
	    ((lacpdu->partner_state & FSM_STATE_SYNCHRONIZATION) !=
		(port->actor_oper_port_state & FSM_STATE_SYNCHRONIZATION)) ||
	    ((lacpdu->partner_state & FSM_STATE_AGGREGATION) !=
		(port->actor_oper_port_state & FSM_STATE_AGGREGATION))) {

		port->ntt = TRUE;
	}
	return;
}


/*
 * _fsm_attach_mux_to_agg: Attach the ports Control Parser/Multiplexer to the
 *                   selected Aggregator.
 *
 * This function causes the ports Control Parser/Multiplexer to be attached to
 * the Aggregator Parser/Multiplexer of the sleceted Aggregator, in preparation
 * for collecting and distributing frames.
 */
static void _fsm_attach_mux_to_agg(struct aggregator *aggr, struct port *port)
{
	return;
}


/*
 * _fsm_detach_mux_from_agg: Detach the ports Control Parser/Multiplexer from the
 *                   current Aggregator.
 *
 * This function causes the ports Control Parser/Multiplexer to be detached from
 * the Aggregator Parser/Multiplexer of the Aggregator to which the port is
 * currently attached.
 */
static void
_fsm_detach_mux_from_agg(struct aggregator *aggr, struct port *port)
{
	return;
}


/*
 * _fsm_enable_collecting_distributing: Stop collecting and distributing frames
 *	             from the attached port.
 *
 * This function causes the Aggregator Parser of the Aggregator to which the port
 * is attached to start collecting frames from the port, and the Aggregator
 * Multiplexer to start distributing frames from the port.
 */
static void
_fsm_enable_collecting_distributing(struct aggregator *aggr, struct port *port)
{
	if (aggr->is_active) {
		LACP_FSM(("Enabling port %d(LAG %d)\n",
			port->actor_port_number, aggr->aggregator_identifier));
	}

	return;
}

/*
 * _fsm_disable_collecting_distributing: Stop collecting and distributing frames
 *	             from the attached port.
 *
 * This function causes the Aggregator Parser of the Aggregator to which the port
 * is attached to Stop collecting frames from the port, and the Aggregator
 * Multiplexer to Stop distributing frames from the port.
 */
static void
_fsm_disable_collecting_distributing(struct aggregator *aggr, struct port *port)
{
	LACP_FSM(("Disabling port %d(LAG %d)\n",
		port->actor_port_number, aggr->aggregator_identifier));

	return;
}


/*
 * LACP state machines
 */
/* LACP Receive machine */
static void
fsm_lacp_receive_machine(struct lacpdu *lacpdu, struct fsm_info *fsm_info, int8 portid)
{
	struct port *port = &(fsm_info->port_info[portid]);
	fsm_rx_states_t last_state;

	ASSERT(portid < MAX_LAG_PORTS);
	RXFSM_LOCK(port);

	/* keep current State Machine state to compare later if it was changed */
	last_state = port->sm_rx_state;

	/* Check if port was reinitialized */
	if (port->sm_vars & FSM_PORT_BEGIN) {
		port->sm_rx_state = FSM_RX_INITIALIZE;		/* next state */
	}
	/* check if port is not enabled */
	else if (!(port->sm_vars & FSM_PORT_BEGIN) &&
		!port->is_linkup && !(port->sm_vars & FSM_PORT_MOVED)) {
		port->sm_rx_state = FSM_RX_PORT_DISABLED;	/* next state */
	}
	/* check if port is disabled */
	else if (port->sm_rx_state == FSM_RX_PORT_DISABLED) {
		if (port->sm_vars & FSM_PORT_MOVED) {
			port->sm_rx_state = FSM_RX_INITIALIZE;	/* next state */
		} else if (port->is_linkup && (port->sm_vars & FSM_PORT_LACP_ENABLED)) {
			port->sm_rx_state = FSM_RX_EXPIRED;	/* next state */
		} else if (port->is_linkup && !(port->sm_vars & FSM_PORT_LACP_ENABLED)) {
			port->sm_rx_state = FSM_RX_LACP_DISABLED;	/* next state */
		}
	}
	/* check if new lacpdu arrived */
	else if (lacpdu &&
		((port->sm_rx_state == FSM_RX_EXPIRED) ||
		(port->sm_rx_state == FSM_RX_DEFAULTED) || (port->sm_rx_state == FSM_RX_CURRENT))) {
		port->sm_rx_state = FSM_RX_CURRENT;
		port->rx_sm_expired = FALSE;
	} else {
		if (port->rx_sm_expired) {
			switch (port->sm_rx_state) {
			case FSM_RX_EXPIRED:
				port->sm_rx_state = FSM_RX_DEFAULTED;	/* next state */
				break;
			case FSM_RX_CURRENT:
				port->sm_rx_state = FSM_RX_EXPIRED;	/* next state */
				break;
			default:
				break;
			}
		} else {
			port->rx_sm_expired = TRUE;
		}
	}

	/* check if the State machine was changed or new lacpdu arrived */
	if ((port->sm_rx_state != last_state) || (lacpdu)) {
		LACP_FSM(("Rx Machine: Port=%d, Last State=%s, Curr State=%s\n",
			port->actor_port_number, dbg_rx_state[last_state],
			dbg_rx_state[port->sm_rx_state]));

		switch (port->sm_rx_state) {
		case FSM_RX_INITIALIZE:
			FSM_CLEAR_STATE(&(port->sm_vars), FSM_PORT_SELECTED);
			_fsm_record_default(port);
			FSM_CLEAR_STATE(&(port->actor_oper_port_state), FSM_STATE_EXPIRED);
			FSM_CLEAR_STATE(&(port->sm_vars), FSM_PORT_MOVED);
			port->sm_rx_state = FSM_RX_PORT_DISABLED;	/* next state */

			if (port->is_fulldplx)
				FSM_SET_STATE(&(port->sm_vars), FSM_PORT_LACP_ENABLED);
			else
				FSM_CLEAR_STATE(&(port->sm_vars), FSM_PORT_LACP_ENABLED);
			/* - Fall Through - */

		case FSM_RX_PORT_DISABLED:
			FSM_CLEAR_STATE(&(port->partner_oper.port_state),
				FSM_STATE_SYNCHRONIZATION);
			FSM_CLEAR_STATE(&(port->sm_vars), FSM_PORT_MATCHED);
			break;
		case FSM_RX_LACP_DISABLED:
			FSM_CLEAR_STATE(&(port->sm_vars), FSM_PORT_SELECTED);
			_fsm_record_default(port);
			FSM_CLEAR_STATE(&(port->partner_oper.port_state), FSM_STATE_AGGREGATION);
			FSM_CLEAR_STATE(&(port->actor_oper_port_state), FSM_STATE_EXPIRED);
			FSM_SET_STATE(&(port->sm_vars), FSM_PORT_MATCHED);
			break;
		case FSM_RX_EXPIRED:
			FSM_CLEAR_STATE(&(port->partner_oper.port_state),
				FSM_STATE_SYNCHRONIZATION);
			FSM_SET_STATE(&(port->partner_oper.port_state), FSM_STATE_LACP_TIMEOUT);
			if (port->sm_rx_state != last_state)
				lacp_timer_start(port->actor_port_number,
					LACP_TIMER_RX_MACHINE, FSM_SHORT_TIMEOUT_TIME, TRUE);
			FSM_SET_STATE(&(port->actor_oper_port_state), FSM_STATE_EXPIRED);
			FSM_CLEAR_STATE(&(port->sm_vars), FSM_PORT_MATCHED);
			break;
		case FSM_RX_DEFAULTED:
			_fsm_update_default_selected(port);
			_fsm_record_default(port);
			FSM_CLEAR_STATE(&(port->actor_oper_port_state), FSM_STATE_EXPIRED);
			FSM_SET_STATE(&(port->sm_vars), FSM_PORT_MATCHED);
			break;
		case FSM_RX_CURRENT:
			_fsm_update_selected(lacpdu, port);
			_fsm_update_ntt(lacpdu, port);
			_fsm_record_pdu(lacpdu, port);
			if (port->actor_oper_port_state & FSM_STATE_LACP_TIMEOUT) {
				if (port->sm_rx_state != last_state)
					lacp_timer_start(port->actor_port_number,
						LACP_TIMER_RX_MACHINE,
						FSM_SHORT_TIMEOUT_TIME, TRUE);
			} else {
				if (port->sm_rx_state != last_state)
					lacp_timer_start(port->actor_port_number,
						LACP_TIMER_RX_MACHINE,
						FSM_LONG_TIMEOUT_TIME, TRUE);
			}
			FSM_CLEAR_STATE(&(port->actor_oper_port_state), FSM_STATE_EXPIRED);
			break;
		default:
			break;
		}
		LACP_FSM(("RX Machine: port->sm_vars 0x%4.4x, port->sm_rx_state %s\n",
			port->sm_vars, dbg_rx_state[port->sm_rx_state]));
	}
	RXFSM_UNLOCK(port);

	return;
}


/* LACP Periodic Transmission machine */
static void
fsm_lacp_periodic_tx_machine(struct fsm_info *fsm_info, int8 portid)
{
	struct port *port = &fsm_info->port_info[portid];
	fsm_periodic_states_t last_state;

	/* keep current state machine state to compare later if it was changed */
	last_state = port->sm_periodic_state;

	/* check if port was reinitialized */
	if (((port->sm_vars & FSM_PORT_BEGIN) ||
		!(port->sm_vars & FSM_PORT_LACP_ENABLED) || !port->is_linkup) ||
	    (!(port->actor_oper_port_state & FSM_STATE_LACP_ACTIVITY) &&
		!(port->partner_oper.port_state & FSM_STATE_LACP_ACTIVITY))) {
		port->sm_periodic_state = FSM_NO_PERIODIC;			/* next state */
	}
	/* check if periodic state machine is NO_PERIODIC */
	else if (port->sm_periodic_state == FSM_NO_PERIODIC) {
		port->sm_periodic_state = FSM_FAST_PERIODIC;			/* next state */
	}
	/* check if periodic state machine is FSM_FAST_PERIODIC */
	else if (port->sm_periodic_state == FSM_FAST_PERIODIC) {
		if (!(port->partner_oper.port_state & FSM_STATE_LACP_TIMEOUT))
			port->sm_periodic_state = FSM_SLOW_PERIODIC;		/* next state */
		else
			port->sm_periodic_state = FSM_PERIODIC_TX;		/* next state */
	/* check if state machine should change state */
	} else {
		switch (port->sm_periodic_state) {
		case FSM_SLOW_PERIODIC:
			port->sm_periodic_state = FSM_PERIODIC_TX;		/* next state */
			break;
		case FSM_PERIODIC_TX:
			if (!(port->partner_oper.port_state & FSM_STATE_LACP_TIMEOUT))
				port->sm_periodic_state = FSM_SLOW_PERIODIC;	/* next state */
			else
				port->sm_periodic_state = FSM_FAST_PERIODIC;	/* next state */
			break;
		default:
			break;
		}
	}

	/* check if the state machine was changed */
	if (port->sm_periodic_state != last_state) {
		LACP_FSM(("Periodic Machine: Port=%d, Last State=%d, Curr State=%d\n",
			port->actor_port_number, last_state, port->sm_periodic_state));
		switch (port->sm_periodic_state) {
		case FSM_NO_PERIODIC:
			break;
		case FSM_FAST_PERIODIC:
			if (port->cur_periodic_timeout != FSM_SHORT_TIMEOUT_TIME) {
				lacp_timer_start(port->actor_port_number,
					LACP_TIMER_PERIODIC_TX, FSM_FAST_PERIODIC_TIME, TRUE);
				port->cur_periodic_timeout = FSM_SHORT_TIMEOUT_TIME;
			}
			break;
		case FSM_SLOW_PERIODIC:
			/* Don't change to slow periodic state until
			 * actor enter to distributing state.
			 */
			if (!(port->lacpdu.actor_state & FSM_STATE_DISTRIBUTING))
				break;

			if (port->cur_periodic_timeout != FSM_LONG_TIMEOUT_TIME) {
				lacp_timer_start(port->actor_port_number,
					LACP_TIMER_PERIODIC_TX, FSM_SLOW_PERIODIC_TIME, TRUE);
				port->cur_periodic_timeout = FSM_LONG_TIMEOUT_TIME;
			}
			break;
		case FSM_PERIODIC_TX:
			port->ntt = TRUE;
			break;
		default:
			break;
		}
	}

	return;
}

/*
 * LACP Selection Logic - Selects  compatible Aggregator for a port.
 */
static void
fsm_lacp_selection_logic(struct fsm_info *fsm_info, int8 portid)
{
	int lag, i, j;
	struct port *port = &fsm_info->port_info[portid], *port_a;
	struct aggregator *sw_aggr, *phy_aggr;

	/* Stage 1: try to find a suit phy_lag */
	if (!_check_port_canbe_select(port, portid))
		return;

	LACP_SELECT(("### Stage 1: try to find a suit phy_lag for port %d\n", portid));
	for (lag = 0; lag < MAX_PHYAGGR_PORTS; lag++) {
		phy_aggr = &fsm_info->phy_aggr[lag];
		if (!phy_aggr->is_active) {
			LACP_SELECT(("@ phy_lag %d is inactive \n", lag));
			continue;
		}

		if (phy_aggr->aggregator_identifier == port->lagid) {
			LACP_SELECT(("@ Port %d is already in phy_lag %d. ports_mask 0x%x\n",
				portid, lag, phy_aggr->ports_mask));
			goto select_exit;
		}

		LACP_SELECT(("@ Check Port %d with phy_lag %d now...\n", portid, lag));
		if (_check_port_agg_match(phy_aggr, port)) {
			LACP_SELECT(("@ S1 MATCH!!! Port %d for phy_lag %d\n", portid, lag));
			/* update port information */
			port->actor_oper_port_key = phy_aggr->actor_oper_aggregator_key;
			port->actor_port_aggregator_identifier =
				phy_aggr->aggregator_identifier;
			port->lagid = phy_aggr->aggregator_identifier;
			port->is_agg = TRUE;
			port->aggr = phy_aggr;
			port->is_phy_agg = TRUE;
			/* mark this port as selected */
			FSM_SET_STATE(&(port->sm_vars), FSM_PORT_SELECTED);
			/* fill port information to lag */
			phy_aggr->lag_ports[portid] = port;
			phy_aggr->num_of_ports++;
			phy_aggr->ports_mask |= (1 << portid);
			LACP_SELECT(("@ SELECTED: phy_aggr->aggregator_identifier %d, "
				"phy_aggr->ports_mask 0x%x\n",
				phy_aggr->aggregator_identifier, phy_aggr->ports_mask));
			lacpc_update_agg(fsm_info->lacpc, phy_aggr->aggregator_identifier,
				phy_aggr->ports_mask);
			goto select_exit;
		} else
			LACP_SELECT(("S1 MISMATCH!!! Port %d for phy_lag %d\n", portid, lag));
	}

	/* Stage 2: try to find a suit active lag (sw lag) */
	LACP_SELECT(("### Stage 2: try to find a suit active sw_lag for port %d\n", portid));
	for (lag = 0; lag < MAX_SWAGGR_PORTS; lag++) {
		sw_aggr = &fsm_info->sw_aggr[lag];
		if (!sw_aggr->is_active)
			continue;

		if (phy_aggr->aggregator_identifier == port->lagid) {
			LACP_SELECT(("@ Port %d is already in sw_aggr %d. ports_mask 0x%x\n",
				portid, lag, sw_aggr->ports_mask));
			goto select_exit;
		}

		LACP_SELECT(("* Check Port %d with sw_lag %d now...\n", portid, lag));
		if (_check_port_agg_match(sw_aggr, port)) {
			LACP_SELECT(("* S2 MATCH!!! Port %d for sw_lag %d\n", portid, lag));
			/* Try to find a free phy_lag */
			for (i = 0; i < MAX_PHYAGGR_PORTS; i++) {
				phy_aggr = &fsm_info->phy_aggr[i];
				if (phy_aggr->is_active) {
					LACP_SELECT(("@ phy_lag %d is active %pM\n",
						lag, phy_aggr->partner_system));
					continue;
				}

				LACP_SELECT(("* Found free phy_lag %d\n", i));
				/* update sw_agg port information */
				for (j = LACP_PORT_1; j < MAX_LAG_PORTS; j++) {
					if (sw_aggr->ports_mask >> j == 0x01) {
						port_a = &fsm_info->port_info[j];
						port_a->lagid = phy_aggr->aggregator_identifier;
						port_a->actor_oper_port_key =
							phy_aggr->actor_oper_aggregator_key;
						port_a->actor_port_aggregator_identifier =
							phy_aggr->aggregator_identifier;
						port_a->aggr = phy_aggr;
						port_a->is_phy_agg = TRUE;
					}
				}

				/* update port information */
				port->actor_oper_port_key = phy_aggr->actor_oper_aggregator_key;
				port->actor_port_aggregator_identifier =
					phy_aggr->aggregator_identifier;
				port->lagid = phy_aggr->aggregator_identifier;
				port->is_agg = TRUE;
				port->aggr = phy_aggr;
				port->is_phy_agg = TRUE;
				/* mark this port as selected */
				FSM_SET_STATE(&(port->sm_vars), FSM_PORT_SELECTED);
				/* fill port information to lag */
				sw_aggr->lag_ports[portid] = port;
				memcpy(phy_aggr->lag_ports, sw_aggr->lag_ports,
					sizeof(struct port *) * MAX_LAG_PORTS);
				phy_aggr->actor_oper_aggregator_key =
					sw_aggr->actor_oper_aggregator_key;
				MAC_ADDRESS_COPY(&(phy_aggr->partner_system),
					&(sw_aggr->partner_system));
				phy_aggr->partner_system_priority =
					sw_aggr->partner_system_priority;
				phy_aggr->partner_oper_aggregator_key =
					sw_aggr->partner_oper_aggregator_key;
				phy_aggr->num_of_ports = 2;
				phy_aggr->ports_mask = sw_aggr->ports_mask | (1 << portid);
				LACP_SELECT(("* SELECTED: phy_aggr->aggregator_identifier %d, "
					"phy_aggr->ports_mask 0x%x\n",
					phy_aggr->aggregator_identifier, phy_aggr->ports_mask));
				phy_aggr->is_active = TRUE;
				/* Try to find a free phy_lag */
				lacpc_update_agg(fsm_info->lacpc, phy_aggr->aggregator_identifier,
					phy_aggr->ports_mask);
				fsm_clear_agg(sw_aggr, lag + 1);
				goto select_exit;
			}
			LACP_SELECT(("* ERROR: There is no free phy_lag!!!\n"));
			goto select_stage3;
		}
	}
	LACP_SELECT(("Find no suit sw_lag for port %d\n", portid));

select_stage3:
	/* Stage 3: try to find a free lag (sw lag) */
	LACP_SELECT(("### Stage 3: try to find a free sw_lag\n"));
	for (lag = 0; lag < MAX_SWAGGR_PORTS; lag++) {
		sw_aggr = &fsm_info->sw_aggr[lag];
		if (sw_aggr->is_active)
			continue;

		/* Found a free lag, fill ports information */
		port->actor_oper_port_key = port->is_fulldplx |
			(port->speed << 1) |
			(sw_aggr->aggregator_identifier <<
				FSM_USER_KEY_INDEX);
		port->actor_port_aggregator_identifier =
			sw_aggr->aggregator_identifier;
		port->lagid = sw_aggr->aggregator_identifier;
		port->is_agg = TRUE;
		port->aggr = sw_aggr;
		port->is_phy_agg = FALSE;
		/* mark this port as selected */
		FSM_SET_STATE(&(port->sm_vars), FSM_PORT_SELECTED);

		/* Fill information to lag */
		sw_aggr->lag_ports[portid] = port;
		sw_aggr->ports_mask = (1 << portid);
		sw_aggr->num_of_ports = 1;
		sw_aggr->actor_oper_aggregator_key =
			port->actor_oper_port_key;
		MAC_ADDRESS_COPY(&(sw_aggr->partner_system),
			&(port->partner_oper.system));
		LACP_SELECT(("sw_aggr->partner_system %pM, "
			"port->actor_port_aggregator_identifier %d, "
			"sw_aggr->ports_mask 0x%x\n",
			sw_aggr->partner_system,
			port->actor_port_aggregator_identifier,
			sw_aggr->ports_mask));
		sw_aggr->partner_system_priority =
			port->partner_oper.system_priority;
		sw_aggr->partner_oper_aggregator_key =
			port->partner_oper.key;
		sw_aggr->is_active = TRUE;

		/* if all aggregator's ports are READY_N == TRUE,
		 * set ready=TRUE in all aggregator's ports
		 * else set ready=FALSE in all aggregator's ports
		 */
		_set_aggr_ports_ready(sw_aggr, port,
			_is_aggr_ready(sw_aggr));
		goto select_exit;
	}
	LACP_SELECT(("ERROR!!! there is no free sw_lag for port %d\n", portid));
	_print_agg(&(fsm_info->sw_aggr[0]));

select_exit:
	if (lacp_msg_level == LACP_SELECT_VAL)
		printf("\n");
	return;
}


/* LACP Mux machine - using coupled coutrol */
static void
fsm_lacp_mux_machine(struct fsm_info *fsm_info, int8 portid)
{
	struct port *port = &fsm_info->port_info[portid];
	struct aggregator *aggr;
	fsm_mux_states_t last_state;

	aggr = port->aggr;
	if (aggr == NULL) {
		LACP_FSM(("ERROR !!! portid %d, aggr is NULL\n", portid));
		return;
	}

	/* keep current State Machine state to compare later if it was changed */
	last_state = port->sm_mux_state;

	if (port->sm_vars & FSM_PORT_BEGIN) {
		LACP_FSM(("Mux Machine: Port=%d, FSM_PORT_BEGIN\n", portid));
		port->sm_mux_state = FSM_MUX_DETACHED;			/* next state */
	} else {
		LACP_FSM(("Mux Machine: %s Port=%d, port->sm_vars 0x%x\n",
			dbg_mux_state[port->sm_mux_state], portid, port->sm_vars));
		switch (port->sm_mux_state) {
		case FSM_MUX_DETACHED:
			if (port->sm_vars & FSM_PORT_SELECTED) {
				port->sm_mux_state = FSM_MUX_WAITING;	/* next state */
			}
			break;
		case FSM_MUX_WAITING:
			/* In order to withhold the Selection Logic to check all ports
			 * READY_N value. Every callback cycle to update ready variable,
			 * we check READY_N and update READY here
			 */
			_set_aggr_ports_ready(aggr, port, _is_aggr_ready(aggr));
			/* if Selected = UNSELECTED */
			if (!(port->sm_vars & FSM_PORT_SELECTED)) {
				FSM_CLEAR_STATE(&(port->sm_vars), FSM_PORT_READY_N);
				port->sm_mux_state = FSM_MUX_DETACHED;	/* next state */
				break;
			}

			FSM_SET_STATE(&(port->sm_vars), FSM_PORT_READY_N);

			/* if Selected = SELECTED and Ready = TRUE */
			if (port->sm_vars & FSM_PORT_READY) {
				port->sm_mux_state = FSM_MUX_ATTACHED;	/* next state */
			}
			break;
		case FSM_MUX_ATTACHED:
			if ((port->sm_vars & FSM_PORT_SELECTED) &&
			    (port->partner_oper.port_state & FSM_STATE_SYNCHRONIZATION)) {
				/* next state */
				port->sm_mux_state = FSM_MUX_COLLECTING_DISTRIBUTING;

			} else if (!(port->sm_vars & FSM_PORT_SELECTED)) {
				FSM_CLEAR_STATE(&(port->sm_vars), FSM_PORT_READY_N);
				/* In order to withhold the Selection Logic to check all ports
				 * READY_N value. Every callback cycle to update ready variable,
				 * we check READY_N and update READY here
				 */
				_set_aggr_ports_ready(aggr, port, _is_aggr_ready(aggr));
				port->sm_mux_state = FSM_MUX_DETACHED;	/* next state */
			}
			break;
		case FSM_MUX_COLLECTING_DISTRIBUTING:
			if (!(port->sm_vars & FSM_PORT_SELECTED) ||
			    !(port->partner_oper.port_state & FSM_STATE_SYNCHRONIZATION)) {
				port->sm_mux_state = FSM_MUX_ATTACHED;	/* next state */
			}
			break;
		default:
			break;
		}
	}

	/* check if the state machine was changed */
	if (port->sm_mux_state != last_state) {
		LACP_FSM(("Mux Machine: Port=%d, Last State=%d, Curr State=%d\n",
			port->actor_port_number, last_state, port->sm_mux_state));
		switch (port->sm_mux_state) {
		case FSM_MUX_DETACHED:
			_fsm_detach_mux_from_agg(aggr, port);
			FSM_CLEAR_STATE(&(port->actor_oper_port_state), FSM_STATE_SYNCHRONIZATION);
			_fsm_disable_collecting_distributing(aggr, port);
			FSM_CLEAR_STATE(&(port->actor_oper_port_state), FSM_STATE_COLLECTING);
			FSM_CLEAR_STATE(&(port->actor_oper_port_state), FSM_STATE_DISTRIBUTING);
			port->ntt = TRUE;
			lacp_timer_start(portid, LACP_TIMER_MUX_MACHINE, FSM_DEFAULT_TIMEOUT, TRUE);
			break;
		case FSM_MUX_WAITING:
			/* Timer is running and timeout is FSM_AGGREGATE_WAIT_TIME.
			 * Doesn't need to do anything here
			 */
			lacp_timer_start(portid, LACP_TIMER_MUX_MACHINE,
				FSM_AGGREGATE_WAIT_TIME, TRUE);
			break;
		case FSM_MUX_ATTACHED:
			_fsm_attach_mux_to_agg(aggr, port);
			FSM_SET_STATE(&(port->actor_oper_port_state), FSM_STATE_SYNCHRONIZATION);
			FSM_CLEAR_STATE(&(port->actor_oper_port_state), FSM_STATE_COLLECTING);
			FSM_CLEAR_STATE(&(port->actor_oper_port_state), FSM_STATE_DISTRIBUTING);
			_fsm_disable_collecting_distributing(aggr, port);
			port->ntt = TRUE;
			break;
		case FSM_MUX_COLLECTING_DISTRIBUTING:
			FSM_SET_STATE(&(port->actor_oper_port_state), FSM_STATE_DISTRIBUTING);
			_fsm_enable_collecting_distributing(aggr, port);
			FSM_SET_STATE(&(port->actor_oper_port_state), FSM_STATE_COLLECTING);
			port->ntt = TRUE;
			break;
		default:
			break;
		}
	}

	return;
}


/* LACP Transmit machine */
static void
fsm_lacp_tx_machine(struct fsm_info *fsm_info, int8 portid)
{
	struct port *port = &fsm_info->port_info[portid];

	LACP_FSM(("fsm_lacp_tx_machine: Port %d NTT %s\n",
		port->actor_port_number, (port->ntt) ? "TRUE" : "FALSE"));
	_print_lacp_state(port);

	/* check if there is something to send */
	if (port->ntt && (port->sm_vars & FSM_PORT_LACP_ENABLED)) {
		_update_lacpdu_from_port(port);

		if (lacp_msg_level & LACP_DUMP_LACPPKT_VAL)
			_dump_lacp_pkt("Tx", portid,
				(void *)(&(port->lacpdu)), sizeof(struct lacpdu));

		if (lacpc_send(fsm_info->lacpc, portid,
			(void *)(&(port->lacpdu)), sizeof(struct lacpdu)) == BCME_OK) {
			/* mark NTT as FALSE, to avoid send the same lacpdu twice. */
			port->ntt = FALSE;
		}
	}

	/* Turn off the BEGIN bit */
	if (port->sm_vars & FSM_PORT_BEGIN)
		FSM_CLEAR_STATE(&(port->sm_vars), FSM_PORT_BEGIN);

	if (lacp_msg_level & LACP_FSM_VAL)
		printf("\n");
	return;
}


static void
fsm_lacp_churn_detection_machine(struct fsm_info *fsm_info, int8 portid)
{
	return;
}


/*
 * Timer callback function
 */
int
lacp_tmr_current_while_timeout(void* ctx, int8 portid)
{
	fsm_lacp_receive_machine(NULL, (struct fsm_info *)ctx, portid);
	return BCME_OK;
}


int
lacp_tmr_periodic_timeout(void* ctx, int8 portid)
{
	fsm_lacp_periodic_tx_machine((struct fsm_info *)ctx, portid);
	return BCME_OK;
}


int
lacp_tmr_selection_timeout(void* ctx, int8 portid)
{
	fsm_lacp_selection_logic((struct fsm_info *)ctx, portid);

	return BCME_OK;
}


int
lacp_tmr_wait_while_timeout(void* ctx, int8 portid)
{
	fsm_lacp_mux_machine((struct fsm_info *)ctx, portid);
	return BCME_OK;
}


int
lacp_tmr_tx_timeout(void* ctx, int8 portid)
{
	fsm_lacp_tx_machine((struct fsm_info *)ctx, portid);
	return BCME_OK;
}


int
lacp_tmr_churn_detection_timeout(void* ctx, int8 portid)
{
	fsm_lacp_churn_detection_machine((struct fsm_info *)ctx, portid);
	return BCME_OK;
}


/*
 * Hal callback function
 */
int32
lacp_hal_recv_skb(void* ctx, int8 portid, void* pkt, int32 pkt_len)
{
	struct port *port;
	struct fsm_info *fsm_info;
	struct lacpdu *lacpdu;

	if (pkt_len >= sizeof(struct lacpdu)) {
		fsm_info = (struct fsm_info *)ctx;
		port = &fsm_info->port_info[portid];
		lacpdu = (struct lacpdu *)pkt;

		if (!(fsm_info->ports_mask & (1 << portid))) {
			LACP_FSM(("Port %d didn't registered.\n", portid));
			return BCME_UNSUPPORTED;
		}


		switch (lacpdu->subtype) {
		case PDU_TYPE_LACPDU:
			if (!port->is_timer_started)
				_start_timer(port, portid);

			if (lacp_msg_level & LACP_DUMP_LACPPKT_VAL)
				_dump_lacp_pkt("Rx", portid, pkt, pkt_len);

			port->rcvd_lacp = TRUE;
			fsm_lacp_receive_machine(lacpdu, fsm_info, portid);
			break;

		case PDU_TYPE_MARKER:
			LACP_FSM(("lacpdu subtype PDU_TYPE_MARKER. NOT support now!!!\n"));
			break;
		}
	}

	return BCME_OK;
}

static void
fsm_initialize_port(struct port *port, int8 portid, char * hostmac, int8 lacpmode, int lacp_fast);

int32
lacp_hal_update_portstatus(void* ctx, int8 portid, uint32 linkup, uint32 speed, uint32 fulldplx)
{
	int i;
	struct fsm_info *fsm_info;
	struct aggregator *aggr;
	struct port *port, *curport;
	uint16 aggr_id;

	LACP_FSM(("Link status change. Port %d, Active %d, speed 0x%x, fulldplx %d\n",
		portid, linkup, speed, fulldplx));
	fsm_info = (struct fsm_info *)ctx;
	port = &fsm_info->port_info[portid];

	if (!(fsm_info->ports_mask & (1 << portid))) {
		LACP_FSM(("Port %d didn't registered.\n", portid));
		return BCME_UNSUPPORTED;
	}

	if (linkup == LACPC_PORT_LINK_UP) {
		LACP_FSM(("Link UP, port %d , is_linkup %d!!!\n", portid, port->is_linkup));
		if (port->is_linkup == TRUE)
			return BCME_OK;
		port->is_linkup = TRUE;
		port->is_fulldplx = (fulldplx == 0) ? FALSE : TRUE;
		port->speed = speed;
		port->actor_oper_port_key = portid;
		port->sm_mux_state = FSM_MUX_DETACHED;
		if (fsm_info->lacp_active_mode)
			_start_timer(port, portid);
	} else {
		LACP_FSM(("Link DOWN, port %d , is_linkup %d!!!\n", portid, port->is_linkup));
		if (port->is_linkup == FALSE)
			return BCME_OK;
		/* link has failed */
		port->is_linkup = FALSE;
		port->actor_oper_port_key = 0;
		if (port->is_timer_started) {
			LACP_FSM(("Stop timer\n"));
			_stop_timer(port, portid);
		}

		/* Clear port LACP state */
		port->rcvd_lacp = FALSE;
		port->sm_vars = FSM_PORT_BEGIN | FSM_PORT_LACP_ENABLED;
		port->lacpdu.actor_state = 0;
		port->lacpdu.partner_state = 0;

		/* Clear aggr this port relate information */
		if (port->is_agg) {
			aggr = port->aggr;
			if (aggr == NULL) {
				LACP_ERROR("Should not be happened!!! port %d aggr is NULL\n",
					portid);
				return BCME_ERROR;
			}

			aggr->lag_ports[portid] = NULL;
			aggr->num_of_ports--;
			aggr->ports_mask &= ~(1 << portid);
			aggr_id = aggr->aggregator_identifier;
			LACP_FSM(("aggr->num_of_ports %d\n", aggr->num_of_ports));
			if (port->is_phy_agg) {
				for (i = LACP_PORT_1; i < MAX_LAG_PORTS; i++) {
					if (aggr->ports_mask >> i == 0x01) {
						curport = &fsm_info->port_info[i];
						LACP_FSM(("PHY LAG %d, curport %d.\n",
							curport->lagid, i));
						curport->rcvd_lacp = FALSE;
						curport->sm_vars = FSM_PORT_BEGIN |
							FSM_PORT_LACP_ENABLED;
						curport->sm_rx_state = FSM_RX_INITIALIZE;
						curport->sm_mux_state = FSM_MUX_DETACHED;
						curport->lacpdu.actor_state = 0;
						curport->lacpdu.partner_state = 0;
						curport->is_agg = FALSE;
						curport->lagid = 0;
						curport->aggr = NULL;
						curport->is_phy_agg = FALSE;
						LACP_FSM(("curport->is_agg %d\n",
							curport->is_agg));
						fsm_clear_agg(aggr, aggr_id);
						LACP_FSM(("PHY aggr->is_active %d, "
							"aggr->num_of_ports %d\n",
							aggr->is_active, aggr->num_of_ports));
					}
				}
			} else {
			/* is sw_agg, should be clear it */
				LACP_FSM(("SW LAG %d, port %d.\n", port->lagid, portid));
				fsm_clear_agg(aggr, aggr_id);
			}
			port->lagid = 0;
			port->is_agg = FALSE;
			LACP_FSM(("port->is_agg %d\n", port->is_agg));
			lacpc_update_agg(fsm_info->lacpc, aggr_id, aggr->ports_mask);
		}
	}

	return BCME_OK;
}


static void
fsm_initialize_port(struct port *port, int8 portid, char * hostmac, int8 lacpmode, int lacp_fast)
{
	static const struct port_params tmpl = {
		.system_priority = 0xffff,
		.key             = 1,
		.port_number     = 1,
		.port_priority   = 0xff,
		.port_state      = 1,
	};
	static const struct lacpdu lacpdu = {
		.subtype			= 0x01,
		.version_number			= 0x01,
		.tlv_type_actor_info		= 0x01,
		.actor_information_length	= 0x14,
		.tlv_type_partner_info		= 0x02,
		.partner_information_length	= 0x14,
		.tlv_type_collector_info	= 0x03,
		.collector_information_length	= 0x10,
		.collector_max_delay		= 0x00,
	};

	if (port) {
		bzero(port, sizeof(struct port));
		port->actor_port_number = portid;
		port->actor_port_priority = 1;
		MAC_ADDRESS_COPY(port->actor_system, hostmac);
		port->actor_system_priority = 1;
		port->actor_port_aggregator_identifier = 0;
		port->ntt = FALSE;
		/* set portid to Actor Port key as temporary,
		 * and will assign correct value after aggregate success
		 */
		port->actor_oper_port_key  = portid;
		port->actor_admin_port_state = FSM_STATE_AGGREGATION;
		port->actor_oper_port_state  = FSM_STATE_AGGREGATION;
		if (lacpmode) {
			FSM_SET_STATE(&(port->actor_admin_port_state), FSM_STATE_LACP_ACTIVITY);
			FSM_SET_STATE(&(port->actor_oper_port_state), FSM_STATE_LACP_ACTIVITY);
		}

		if (lacp_fast)
			FSM_SET_STATE(&(port->actor_oper_port_state), FSM_STATE_LACP_TIMEOUT);

		memcpy(&port->partner_admin, &tmpl, sizeof(tmpl));
		memcpy(&port->partner_oper, &tmpl, sizeof(tmpl));

		port->is_linkup = FALSE;
		/* ======= private parameters ======= */
		port->sm_vars = FSM_PORT_BEGIN | FSM_PORT_LACP_ENABLED;
		port->sm_rx_state = 0;
		port->sm_periodic_state = 0;
		port->sm_mux_state = 0;

		memcpy(&port->lacpdu, &lacpdu, sizeof(lacpdu));
	}

	return;
}


static void
fsm_clear_agg(struct aggregator *aggr, int aggr_id)
{
	LACP_FSM(("fsm_clear_agg: LAG %d\n", aggr_id));
	bzero(aggr, sizeof(struct aggregator));
	aggr->aggregator_identifier = aggr_id;
	return;
}


static uint32
fsm_get_lacp_ports_config(void)
{
	char port[] = "XXXX", *next;
	const char *ports, *cur;
	int pid, len;
	uint32 portmask = 0;

	/* get lacp ports defeinitions from nvram */
	ports = getvar(NULL, "lacpports");
	LACP_MSG("lacpports %s\n", ports);
	if (ports) {
		for (cur = ports; cur; cur = next) {
			/* tokenize the port list */
			while (*cur == ' ')
				cur ++;
			next = bcmstrstr(cur, " ");
			len = next ? next - cur : strlen(cur);
			if (!len)
				break;
			if (len > sizeof(port) - 1)
				len = sizeof(port) - 1;
			strncpy(port, cur, len);
			port[len] = 0;

			/* make sure port # is within the range */
			pid = bcm_atoi(port);
			if (pid >= MAX_LAG_PORTS) {
				LACP_ERROR("port %d is out of range[0-%d]\n",
					pid, MAX_LAG_PORTS);
				continue;
			}
			portmask |= (1 << pid);
		}
	}
	return portmask;
}

void
fsm_get_lacp_status(void* ctx, struct fsm_status* fsm_status)
{
	int i;
	struct fsm_info *fsm_info;
	struct aggregator *aggr;
	struct port *port;

	fsm_info = (struct fsm_info *)ctx;
	fsm_status->lacp_active_mode = fsm_info->lacp_active_mode;
	for (i = 0; i < MAX_PHYAGGR_PORTS; i++) {
		aggr = &fsm_info->phy_aggr[i];
		fsm_status->lag_actived[i] = aggr->is_active;
		fsm_status->lag_ports_mask[i] = aggr->ports_mask;
	}
	for (i = 0; i < MAX_LAG_PORTS; i++) {
		port = &fsm_info->port_info[i];
		fsm_status->port_is_agg[i] = port->is_phy_agg;
		fsm_status->port_lag[i] = port->lagid;
		fsm_status->port_actor_state[i] = port->lacpdu.actor_state;
		fsm_status->port_partner_state[i] = port->lacpdu.partner_state;
	}

	return;
}

void
fsm_get_lacp_lag_status(void* ctx, struct fsm_lag_status* fsm_lag_status)
{
	struct fsm_info *fsm_info;

	fsm_info = (struct fsm_info *)ctx;

	memcpy(&fsm_lag_status->sw_aggr[0], &fsm_info->sw_aggr[0],
		sizeof(struct aggregator) * MAX_SWAGGR_PORTS);
	memcpy(&fsm_lag_status->phy_aggr[0], &fsm_info->phy_aggr[0],
		sizeof(struct aggregator) * MAX_PHYAGGR_PORTS);

	return;
}

/*
 * Initilize lacp_fsm.
 */
void*
fsm_init(void *lacpc, osl_t *osh)
{
	int i;
	struct fsm_info *fsm_info;
	struct port *port;
	int8 lacpmode;

	printf("### LACP: %s %s version %s\n", __DATE__, __TIME__, EPI_VERSION_STR);

	if (initialized) {
		LACP_FSM(("initialized\n"));
		return NULL;
	}

	fsm_info = (struct fsm_info *)MALLOCZ(osh, sizeof(struct fsm_info));
	if (fsm_info == NULL) {
		LACP_ERROR("out of memory, malloced %d bytes\n", MALLOCED(osh));
		return NULL;
	}

	lacpc_get_hostmac(lacpc, fsm_info->hostmac);

	/* get LACP mode setting from nvram */
	lacpmode = getintvar(NULL, "lacpmode");
	fsm_info->lacp_active_mode = (lacpmode == 1) ? TRUE : FALSE;

	/* get LACP group port settings and initilize agg */
	fsm_info->ports_mask = fsm_get_lacp_ports_config();
	LACP_MSG("fsm_info->ports_mask 0x%x\n", fsm_info->ports_mask);
	for (i = 0; i < MAX_SWAGGR_PORTS; i++) {
		fsm_clear_agg(&fsm_info->sw_aggr[i], i + 1);
	}
	for (i = 0; i < MAX_PHYAGGR_PORTS; i++) {
		fsm_clear_agg(&fsm_info->phy_aggr[i], i + 1);
	}

	for (i = LACP_PORT_1; i < MAX_LAG_PORTS; i++) {
		if (fsm_info->ports_mask & (1 << i)) {
			LACP_MSG("Port %d initialize\n", i);
			port = &(fsm_info->port_info[i]);
			RXFSM_LOCK_INIT(port);

			fsm_initialize_port(port, i, fsm_info->hostmac,
				fsm_info->lacp_active_mode, TRUE);
			/* register timer for lacp receive machine */
			lacp_timer_register(i, LACP_TIMER_RX_MACHINE,
				lacp_tmr_current_while_timeout, (void*)fsm_info);
			/* register timer for lacp periodic tx machine */
			lacp_timer_register(i, LACP_TIMER_PERIODIC_TX,
				lacp_tmr_periodic_timeout, (void*)fsm_info);
			/* register timer for lacp selection logic */
			lacp_timer_register(i, LACP_TIMER_SELECT_LOGIC,
				lacp_tmr_selection_timeout, (void*)fsm_info);
			/* register timer for lacp mux machine */
			lacp_timer_register(i, LACP_TIMER_MUX_MACHINE,
				lacp_tmr_wait_while_timeout, (void*)fsm_info);
			/* register timer for lacp tx machine */
			lacp_timer_register(i, LACP_TIMER_TX_MACHINE,
				lacp_tmr_tx_timeout, (void*)fsm_info);
			/* register timer for lacp churn detection machine */
			lacp_timer_register(i, LACP_TIMER_CHUNK_DECTION,
				lacp_tmr_churn_detection_timeout, (void*)fsm_info);
			fsm_info->lastport = i;
		}
	}
	LACP_MSG("lastport %d\n", fsm_info->lastport);

	fsm_info->osh = osh;
	fsm_info->lacpc = lacpc;
	/* register lacp rcv callback function */
	lacpc_register_rcv_handler(lacpc, (void*)fsm_info, lacp_hal_recv_skb);
	/* register port change callback function */
	lacpc_register_portchg_handler(lacpc, (void*)fsm_info, lacp_hal_update_portstatus);

	initialized = TRUE;
	return (void*)fsm_info;
}
EXPORT_SYMBOL(fsm_init);

int
fsm_deinit(void *ctx)
{
	int i;
	struct fsm_info *fsm_info;

	if (!initialized) {
		LACP_FSM(("%s: NOT initialized\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (!ctx) {
		LACP_ERROR("ctx ERROR\n");
		return BCME_ERROR;
	}

	fsm_info = (struct fsm_info *)ctx;
	for (i = 0; i < MAX_LAG_PORTS; i++) {
		if (fsm_info->ports_mask & (1 << i)) {
			LACP_MSG("Port %d uninitialize\n", i);
			if (fsm_info->port_info[i].is_timer_started)
				_stop_timer(&fsm_info->port_info[i], i);
			fsm_info->port_info[i].rcvd_lacp = FALSE;
			lacp_timer_unregister(i, LACP_TIMER_RX_MACHINE);
			lacp_timer_unregister(i, LACP_TIMER_PERIODIC_TX);
			lacp_timer_unregister(i, LACP_TIMER_SELECT_LOGIC);
			lacp_timer_unregister(i, LACP_TIMER_MUX_MACHINE);
			lacp_timer_unregister(i, LACP_TIMER_TX_MACHINE);
			lacp_timer_unregister(i, LACP_TIMER_CHUNK_DECTION);
		}
	}
	MFREE(fsm_info->osh, fsm_info, sizeof(struct fsm_info));

	initialized = FALSE;
	return BCME_OK;
}
