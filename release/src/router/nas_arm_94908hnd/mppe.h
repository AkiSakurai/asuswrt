/*
 * mppe.h Broadcom support for Microsoft Point-to-Point Encryption Protocol.
 *
 * Broadcom Proprietary and Confidential. Copyright (C) 2016,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 * $Id: mppe.h 241182 2011-02-17 21:50:03Z $
 */

#if !defined(_MPPE_H_)
#define _MPPE_H_

void mppe_crypt(unsigned char salt[2], unsigned char *text, int text_len,
                unsigned char *key, int key_len, unsigned char vector[16],
                int encrypt);

#endif /* !defined(_MPPE_H_) */
