/**
 * 802.11d (support for additional regulatory domains) module header file
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_11d.h 453919 2014-02-06 23:10:30Z $
*/


#ifndef _wlc_11d_h_
#define _wlc_11d_h_


/* APIs */
#ifdef WL11D
#if defined(CNTRY_DEFAULT) && defined(WLC_HIGH)
/* wl cntry_default feature */
#define WLC_CNTRY_DEFAULT_ENAB(wlc) \
	((wlc)->m11d != NULL && wlc_11d_cntry_default_enabled((wlc)->m11d))
#else
#define WLC_CNTRY_DEFAULT_ENAB(wlc) FALSE
#endif /* CNTRY_DEFAULT */
#ifdef LOCALE_PRIORITIZATION_2G
/* wl cntry_default feature */
#define WLC_LOCALE_PRIORITIZATION_2G_ENABLED(wlc) \
	((wlc)->m11d != NULL && wlc_11d_locale_prioritization_2g_enabled((wlc)->m11d))
#else
#define WLC_LOCALE_PRIORITIZATION_2G_ENABLED(wlc) FALSE
#endif /* LOCALE_PRIORITIZATION_2G */


/* module */
extern wlc_11d_info_t *wlc_11d_attach(wlc_info_t *wlc);
extern void wlc_11d_detach(wlc_11d_info_t *m11d);

extern void wlc_11d_scan_complete(wlc_11d_info_t *m11d, int status);
extern void wlc_11d_scan_start(wlc_11d_info_t *m11d);
extern void wlc_11d_scan_result(wlc_11d_info_t *m11d, wlc_bss_info_t *bi,
                                uint8 *bcn_prb, uint bcn_prb_len);

/* actions */
extern void wlc_11d_adopt_country(wlc_11d_info_t *m11d, char *country_str, bool adopt_country);
extern void wlc_11d_reset_all(wlc_11d_info_t *m11d);

/* accessors */
#ifdef CNTRY_DEFAULT
extern bool wlc_11d_cntry_default_enabled(wlc_11d_info_t *m11d);
#endif /* CNTRY_DEFAULT */
#ifdef LOCALE_PRIORITIZATION_2G
extern bool wlc_11d_locale_prioritization_2g_enabled(wlc_11d_info_t *m11d);
#endif /* LOCALE_PRIORITIZATION_2G */
extern bool wlc_11d_autocountry_adopted(wlc_11d_info_t *m11d);
extern void wlc_11d_set_autocountry_default(wlc_11d_info_t *m11d, const char *country_abbrev);
extern const char *wlc_11d_get_autocountry_default(wlc_11d_info_t *m11d);
extern int wlc_11d_compatible_country(wlc_11d_info_t *m11d, const char *country_abbrev);
extern void wlc_11d_reset_autocountry_adopted(wlc_11d_info_t *m11d);
extern void wlc_11d_set_autocountry_adopted(wlc_11d_info_t *m11d, const char *country_abbrev);
extern const char *wlc_11d_get_autocountry_adopted(wlc_11d_info_t *m11d);
extern bool wlc_11d_autocountry_scan_learned(wlc_11d_info_t *m11d);
extern const char *wlc_11d_get_autocountry_scan_learned(wlc_11d_info_t *m11d);
extern void wlc_11d_set_autocountry_scan_learned(wlc_11d_info_t *m11d, const char *country_abbrev);

#else /* !WL11D */
#define WLC_CNTRY_DEFAULT_ENAB(wlc) FALSE
#define WLC_LOCALE_PRIORITIZATION_2G_ENABLED(wlc) FALSE

#define wlc_11d_attach(wlc) NULL
#define wlc_11d_detach(m11d) do {} while (0)

#define wlc_11d_scan_complete(m11d, status) do {} while (0)
#define wlc_11d_scan_start(m11d) do {} while (0)
#define wlc_11d_scan_result(m11d, bi, bcn_prb, bcn_prb_len) do {} while (0)

#define wlc_11d_adopt_country(m11d, country_str, adopt_country) BCM_REFERENCE(adopt_country)
#define wlc_11d_reset_all(m11d) do {} while (0)

#define wlc_11d_cntry_default_enabled(m11d) FALSE
#define wlc_11d_autocountry_adopted(m11d) FALSE
#define wlc_11d_set_autocountry_default(m11d, country_abbrev) BCM_REFERENCE(country_abbrev)
#define wlc_11d_get_autocountry_default(m11d) NULL
#define wlc_11d_compatible_country(m11d, country_abbrev) FALSE
#define wlc_11d_reset_autocountry_adopted(m11d) do {} while (0)
#define wlc_11d_set_autocountry_adopted(m11d, country_abbrev) do {} while (0)
#define wlc_11d_get_autocountry_adopted(m11d) NULL
#define wlc_11d_autocountry_scan_learned(m11d) FALSE
#define wlc_11d_get_autocountry_scan_learned(m11d) NULL
#define wlc_11d_set_autocountry_scan_learned(m11d, country_abbrev) do {} while (0)

#endif /* !WL11D */

#endif /* _wlc_11d_h_ */
