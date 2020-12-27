/*!
 * *************************************************************
 *
 * Copyright(c) 2011, Works Systems, Inc. All rights reserved.
 *
 * This software is supplied under the terms of a license agreement
 * with Works Systems, Inc, and may not be copied nor disclosed except
 * in accordance with the terms of that agreement.
 *
 * *************************************************************
 */

/*!
 * \file hex.c
 *
 */
#include "hex.h"

void binary2hexstr( unsigned char *bin, int bin_len, char *buf, int buf_len )
{
    char *hex_alpha = "0123456789abcdef";
    int i;

    for( i = 0; i < bin_len && ( 2 * i + 1 ) < buf_len; i++ ) {
        buf[2 * i] = hex_alpha[bin[i] >> 4];
        buf[2 * i + 1] = hex_alpha[bin[i] & 0x0f];
    }

    buf[2 * i] = '\0';
}
