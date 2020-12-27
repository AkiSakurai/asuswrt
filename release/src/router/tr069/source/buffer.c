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
 * \file buffer.c
 *
 * \brief Implement a dynamic buffer to hold a message(string)
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tr.h"
#include "buffer.h"
#include "log.h"

#define BUFFER_INCREASE 512

void destroy_buffer( struct buffer *b )
{
    if( b && b->data ) {
        free( b->data );
        b->data = NULL;
        b->buf_len = 0;
        b->data_len = 0;
    }
}

void init_buffer( struct buffer *b )
{
    if( b ) {
        b->buf_len = 0;
        b->data_len = 0;
        b->data = NULL;
    }
}

void trim_buffer( struct buffer *b, int len )
{
    if( b ) {
        if( b->data && b->buf_len >= len && b->data_len >= len ) {
            b->data_len -= len;
            b->data[b->data_len] = '\0';
        }
    }
}


void reset_buffer( struct buffer *b )
{
    if( b ) {
        b->data_len = 0;

        if( b->data ) {
            b->data[0] = '\0';
        }
    }
}


int vpush_buffer( struct buffer *b, const char *fmt, va_list ap )
{
    char *np;
    va_list copy;

    for( ;; ) {
        int n;
        va_copy(copy, ap);

        if( b->data == NULL ) {
            char data[1];
            b->data_len = 0;
            b->buf_len = 0;
            n = vsnprintf( data, sizeof( data ), fmt, copy );
            va_end(copy);
        } else {
            n = vsnprintf( b->data + b->data_len, b->buf_len - b->data_len, fmt, copy );
            va_end(copy);
        }

        /* If that worked, return the string. */
#ifdef __OS_ECOS__

        if( n > -1 && n < b->buf_len - b->data_len - 1 ) {
#else

        if( n > -1 && n < b->buf_len - b->data_len ) {
#endif
            b->data_len += n;
            return 0;
        } else if( b->data ) {
            b->data[b->data_len] = '\0';
        }

        /*The buffer space not enough*/
        if( n > -1 ) {
            np = realloc( b->data, b->buf_len + n + 2 );
        } else {
            np = realloc( b->data, b->buf_len + BUFFER_INCREASE );
        }

        if( np ) {
            b->data = np;

            if( n > -1 ) {
                b->buf_len += ( n + 2 );
            } else {
                b->buf_len += BUFFER_INCREASE;
            }
        } else {
            tr_log( LOG_ERROR, "Out of memory!" );
            return -1;
        }
    }
}

int push_buffer( struct buffer *b, const char *fmt, ... )
{
    va_list ap;
    int res;
    va_start( ap, fmt );
    res = vpush_buffer( b, fmt, ap );
    va_end( ap );
    return res;
}

int bpush_buffer( struct buffer *b, const void *data, int len )
{
    if( len <= 0 ) {
        return 0;
    }

    if( b->data == NULL ) {
        b->data = malloc( len + 1 );

        if( b->data == NULL ) {
            tr_log( LOG_ERROR, "Out of memory!" );
            return -1;
        }

        b->buf_len = len + 1;
        b->data_len = 0;
        b->data[0] = '\0';
    } else if( b->buf_len < b->data_len + len + 1 ) {
        char *tmp;
        tmp = realloc( b->data, b->data_len + len + 1 );

        if( tmp == NULL ) {
            tr_log( LOG_ERROR, "Out of memory!" );
            return -1;
        }

        b->data = tmp;
        b->buf_len = b->data_len + len + 1;
    }

    memmove( b->data + b->data_len, data, len );
    b->data_len += len;
    b->data[b->data_len] = '\0';
    return 0;
}
