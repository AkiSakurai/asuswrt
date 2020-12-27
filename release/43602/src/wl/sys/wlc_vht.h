/*
 * Common (OS-independent) portion of
 * Broadcom 802.11 Networking Device Driver
 *
 * VHT support
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_vht.h 467043 2014-04-02 07:49:51Z $
 */


#ifndef _wlc_vht_h_
#define _wlc_vht_h_
#ifdef WL11AC

#include "osl.h"
#include "typedefs.h"
#include "bcmwifi_channels.h"
#include "proto/802.11.h"
#include "wlc_types.h"
#include "wlc_bsscfg.h"


/* module entries */
extern wlc_vht_info_t *wlc_vht_attach(wlc_info_t *wlc);
extern void wlc_vht_detach(wlc_vht_info_t *vhti);
extern void wlc_vht_init_defaults(wlc_vht_info_t *vhti);

/* Update tx and rx mcs maps */
extern void wlc_vht_update_mcs_cap(wlc_vht_info_t *vhti);

/* IE mgmt */
extern vht_cap_ie_t * wlc_read_vht_cap_ie(wlc_vht_info_t *vhti, uint8 *tlvs, int tlvs_len,
	vht_cap_ie_t* cap_ie);
extern vht_op_ie_t * wlc_read_vht_op_ie(wlc_vht_info_t *vhti, uint8 *tlvs, int tlvs_len,
	vht_op_ie_t* op_ie);
extern uint8 *wlc_read_vht_features_ie(wlc_vht_info_t *vhti,  uint8 *tlvs,
	int tlvs_len, uint8 *rate_mask, int *prop_tlv_len);

/* return valid vht chanspec given the op_ie and ht_chanspec */
extern chanspec_t wlc_vht_chanspec(wlc_vht_info_t *vhti, vht_op_ie_t *op_ie,
	chanspec_t ht_chanspec, bool oper_mode_enab, uint8 oper_mode);
extern void wlc_vht_upd_rate_mcsmap(wlc_vht_info_t *vhti, struct scb *scb);
extern void wlc_vht_update_sgi_rx(wlc_vht_info_t *vhti, uint int_val);
extern void wlc_vht_update_ampdu_cap(wlc_vht_info_t *vhti, uint8 vht_rx_factor);

/* get/set current cap info this node uses */
extern void wlc_vht_set_ldpc_cap(wlc_vht_info_t *vhti, bool enab);

extern void wlc_vht_set_tx_stbc_cap(wlc_vht_info_t *vhti, bool enab);

extern void wlc_vht_set_rx_stbc_cap(wlc_vht_info_t *vhti, int val);

extern uint32 wlc_vht_get_cap_info(wlc_vht_info_t *vhti);

/* per SCB functions */
extern void wlc_vht_bcn_scb_upd(wlc_vht_info_t *vhti, int band, struct scb *scb,
	ht_cap_ie_t *ht_cap, vht_cap_ie_t *vht_cap, vht_op_ie_t *vht_op,
	uint8 vht_ratemask);
extern void wlc_vht_update_scb_state(wlc_vht_info_t *vhti, int band, struct scb *scb,
	ht_cap_ie_t *cap_ie, vht_cap_ie_t *vht_cap_ie, vht_op_ie_t *vht_op_ie,
	uint8 vht_ratemask);

/* perform per scb oper mode changes */
extern void wlc_vht_update_scb_oper_mode(wlc_vht_info_t *vhti, struct scb *scb,
	uint8 mode);

/* get the ratemask corresponding to the scb */
extern uint8 wlc_vht_get_scb_ratemask(wlc_vht_info_t *vhti, struct scb *scb);

/* get the vhtflags corresponding to the scb */
extern uint16 wlc_vht_get_scb_flags(wlc_vht_info_t *vhti, struct scb *scb);

/* get oper mode */
extern bool wlc_vht_get_scb_opermode_enab(wlc_vht_info_t *vhti, struct scb *scb);
extern bool wlc_vht_set_scb_opermode_enab(wlc_vht_info_t *vhti, struct scb *scb, bool set);
extern uint8 wlc_vht_get_scb_opermode(wlc_vht_info_t *vhti, struct scb *scb);

extern uint8 wlc_vht_get_scb_ratemask_per_band(wlc_vht_info_t *vhti, struct scb *scb);

/* agg parameters */
extern uint16 wlc_vht_get_scb_amsdu_mtu_pref(wlc_vht_info_t *vhti, struct scb *scb);
extern uint8 wlc_vht_get_scb_ampdu_max_exp(wlc_vht_info_t *vhti, struct scb *scb);
extern void wlc_vht_upd_txbf_cap(wlc_vht_info_t *vhti, bool bfr, bool bfe);
extern void wlc_vht_upd_txbf_virtif_cap(wlc_vht_info_t *vhti, bool bfr, bool bfe, uint32 *cap);
/* ht/vht operating mode (11ac) */
extern void wlc_frameaction_vht(wlc_vht_info_t *vhti, uint action_id, struct scb *scb,
struct dot11_management_header *hdr, uint8 *body, int body_len);
#else
/* empty macros to avoid having to use WL11AC flags everywhere */
#define wlc_vht_get_scb_amsdu_mtu_pref(x, y) 0
#define wlc_vht_update_ampdu_cap(x, y)
#define wlc_vht_update_sgi_rx(x, y)
#define wlc_frameaction_vht(a, b, c, d, e, f)
#define wlc_vht_bcn_scb_upd(a, b, c, d, e, f, g)
#define wlc_vht_chanspec(a, b, c, d, e)
#define wlc_vht_set_ldpc_cap(x, y)
#define wlc_vht_init_defaults(x)
#define wlc_vht_set_rx_stbc_cap(x, y)
#define wlc_vht_update_mcs_cap(x)
#define wlc_vht_get_cap_info(x) 0
#define wlc_vht_update_mcs_cap(x)
#define wlc_read_vht_cap_ie(a, b, c, d) (NULL)
#define wlc_read_vht_cap_ie(a, b, c, d) (NULL)
#define wlc_read_vht_features_ie(a, b, c, d, e) (NULL)
#define wlc_vht_prep_rate_info(a, b, c, d, e) (FALSE)
#endif /* WL11AC */
#endif /* _wlc_vht_h_ */
