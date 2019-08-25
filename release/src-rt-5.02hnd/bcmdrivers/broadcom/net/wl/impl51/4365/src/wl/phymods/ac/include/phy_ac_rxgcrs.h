/*
 * ACPHY Rx Gain Control and Carrier Sense module interface (to other PHY modules).
 *
 * Broadcom Proprietary and Confidential. Copyright (C) 2017,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 * $Id$
 */

#ifndef _phy_ac_rxgcrs_h_
#define _phy_ac_rxgcrs_h_

#include <phy_api.h>
#include <phy_ac.h>
#include <phy_rxgcrs.h>

/* forward declaration */
typedef struct phy_ac_rxgcrs_info phy_ac_rxgcrs_info_t;

/* register/unregister ACPHY specific implementations to/from common */
phy_ac_rxgcrs_info_t *phy_ac_rxgcrs_register_impl(phy_info_t *pi,
	phy_ac_info_t *aci, phy_rxgcrs_info_t *cmn_info);
void phy_ac_rxgcrs_unregister_impl(phy_ac_rxgcrs_info_t *ac_info);


/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */

/* ********************************* */
/*		Rx gain related definitions		*/
/* ********************************* */
/* gainctrl */
/* stages: elna, lna1, lna2, mix, bq0, bq1, dvga (tiny only) */
#define ACPHY_MAX_RX_GAIN_STAGES 7
#define ACPHY_INIT_GAIN 69
#define ACPHY_HI_GAIN 48
#define ACPHY_CRSMIN_DEFAULT 54
#define ACPHY_CRSMIN_GAINHI 32
#define ACPHY_MAX_LNA1_IDX 5
#define ACPHY_4365_MAX_LNA1_IDX 4
#define ACPHY_ILNA2G_MAX_LNA2_IDX 6
#define ACPHY_ELNA2G_MAX_LNA2_IDX 4
#define ACPHY_ELNA2G_MAX_LNA2_IDX_L 5
#define ACPHY_ILNA5G_MAX_LNA2_IDX 6
#define ACPHY_ELNA5G_MAX_LNA2_IDX 6
#define ACPHY_LESISCALE_DEFAULT 64
/* Following MACROS will be used by GBD only.
 * Tiny gain control has a local array of INIT and clip gains.
 */
#define ACPHY_INIT_GAIN_TINY 61
#define ACPHY_HI_GAIN_TINY 37
/* End of GBD used MACRO
 */

#define ACPHY_INIT_GAIN_4365_2G 67
#define ACPHY_INIT_GAIN_4365_5G 64

#define ACPHY_20693_MAX_LNA2_IDX 3
#define ACPHY_4365_MAX_LNA2_IDX 2
#define MAX_ANALOG_RX_GAIN_TINY 55
/* Programming this value into gainlimittable for an index prevents AGC from using it */
#define GAINLIMIT_MAX_OUT 127

#define ELNA_ID 0
#define LNA1_ID 1
#define LNA2_ID 2
#define TIA_ID	3
#define BQ0_ID 4
#define BQ1_ID 5
#define DVGA_ID 6

#define ELNA_OFFSET              0
#define LNA1_OFFSET		8
#define LNA2_OFFSET		16
#define TIA_OFFSET		32
#define BQ0_OFFSET               96
#define BQ1_OFFSET              112
#define DVGA1_OFFSET	       112

#define GAINLMT_TBL_BAND_OFFSET 64

/* PAPD MODE CONFIGURATION */
#define PAPD_LMS 0
#define PAPD_ANALYTIC 1
#define PAPD_ANALYTIC_WO_YREF 2

typedef struct {
	uint8 elna;
	uint8 trloss;
	uint8 elna_bypass_tr;
	uint8 lna1byp;
} acphy_fem_rxgains_t;

typedef struct acphy_rxgainctrl_params {
	int8  gaintbl[ACPHY_MAX_RX_GAIN_STAGES][15];
	uint8 gainbitstbl[ACPHY_MAX_RX_GAIN_STAGES][15];
} acphy_rxgainctrl_t;

typedef enum {
	GET_LNA_GAINCODE,
	GET_LNA_ROUT
} acphy_lna_gain_rout_t;

extern uint8 wlc_phy_rxgainctrl_encode_gain_acphy(phy_info_t *pi, uint8 clipgain,
                                                  uint8 core, int8 gain_dB,
                                                  bool trloss, bool lna1byp, uint8 *gidx);
extern uint8 wlc_phy_get_lna_gain_rout(phy_info_t *pi, uint8 idx,
	acphy_lna_gain_rout_t type);

#ifndef WLC_DISABLE_ACI
extern void wlc_phy_desense_apply_acphy(phy_info_t *pi, bool apply_desense);
extern void wlc_phy_desense_calc_total_acphy(phy_info_t *pi);
extern void wlc_phy_desense_btcoex_acphy(phy_info_t *pi, int32 mode);
#ifdef BCMLTECOEX
extern void wlc_phy_desense_ltecx_acphy(phy_info_t * pi, int32 mode);
#endif /* BCMLTECOEX */
#endif /* WLC_DISABLE_ACI */

extern void wlc_phy_rxgainctrl_gainctrl_acphy(phy_info_t *pi);
extern void wlc_phy_upd_lna1_lna2_gains_acphy(phy_info_t *pi);
extern void wlc_phy_upd_lna1_lna2_gaintbls_acphy(phy_info_t *pi, uint8 lna12);
extern void wlc_phy_upd_lna1_lna2_gainlimittbls_acphy(phy_info_t *pi, uint8 lna12);
extern void wlc_phy_rfctrl_override_rxgain_acphy(phy_info_t *pi, uint8 restore,
	rxgain_t rxgain[], rxgain_ovrd_t rxgain_ovrd[]);
extern uint8 wlc_phy_calc_extra_init_gain_acphy(phy_info_t *pi, uint8 extra_gain_3dB,
	rxgain_t rxgain[]);
extern uint8 wlc_phy_get_max_lna_index_acphy(phy_info_t *pi, uint8 lna);
extern void wlc_phy_rxgainctrl_gainctrl_acphy_tiny(phy_info_t *pi, uint8 init_desense);

/* ************************************* */
/*		Carrier Sense related definitions		*/
/* ************************************* */

extern void wlc_phy_force_crsmin_acphy(phy_info_t *pi, void *p);
extern void wlc_phy_force_lesiscale_acphy(phy_info_t *pi, void *p);
extern void wlc_phy_crs_min_pwr_cal_acphy(phy_info_t *pi, uint8 crsmin_cal_mode);
int8 wlc_phy_get_avg_noisepwr_acphy(int8 noisepwr_array[]);
extern void wlc_phy_set_crs_min_pwr_acphy(phy_info_t *pi, uint8 ac_th, int8 *offset);
extern void wlc_phy_set_lesiscale_acphy(phy_info_t *pi, int8 *lesi_scale);
extern void wlc_phy_stay_in_carriersearch_acphy(phy_info_t *pi, bool enable);
extern void wlc_phy_force_gainlevel_acphy(phy_info_t *pi, int16 int_val);
extern void wlc_phy_set_srom_eu_edthresh_acphy(phy_info_t *pi);
extern void wlc_phy_lesi_acphy(phy_info_t *pi, bool on);
#endif /* _phy_ac_rxgcrs_h_ */
