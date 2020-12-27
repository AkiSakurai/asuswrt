/*
 * GTK offload	definitions
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_gtkol.h Harishv $
 */

/**
 * GTK = Group Key Rotation
 */


#ifndef _wlc_gtkol_h_
#define _wlc_gtkol_h_

extern wlc_dngl_ol_gtk_info_t *
wlc_dngl_ol_gtk_attach(wlc_dngl_ol_info_t *wlc_dngl_ol);
extern void wlc_dngl_ol_gtk_send_proc(wlc_dngl_ol_gtk_info_t *gtk_ol,
	void *buf, int len);
bool
wlc_dngl_ol_eapol(wlc_dngl_ol_gtk_info_t *ol_gtk,
	eapol_header_t *eapol_hdr, bool encrypted);

#endif /* GTK_OFFLOADS */
