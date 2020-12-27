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
 * Prerequisites:
 * block cipher algorithm;
 * key K;
 * counter generation function;
 * formatting function;
 * valid MAC length Tlen.
 * Input:
 * nonce N;
 * associated data A;
 * purported ciphertext C of length Clen bits;
 * Output:
 * either the payload P or INVALID.
 */
#ifdef TR069_WIB

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "pub.h"

static unsigned char flagCounter;
static int q, c, Clen, a, Alen, t, Tlen, n, Nlen;

static void init_args( int n_len, int a_len, int c_len, int t_len )
{
    Nlen = n_len;
    n = n_len / 8;
    q =  blockSize - 1 - n;
    flagCounter = ( unsigned char ) q - 1;
    Clen = c_len;
    c = Clen / 8;
    Alen = a_len;
    a = Alen / 8;
    Tlen = t_len;
    t = Tlen / 8;
}

static int formatting( unsigned char *N, unsigned char *A, unsigned char *P, struct point **p, int *row )
{
    int i;
    int j;
    unsigned char *Q = NULL; /* This */
    int clen_tlen_bytes;
    int temp ;
    unsigned char flag_B0 = 0;
    unsigned char b0[blockSize];
    int row_a = 0, row_p = 0;
    struct point *a_blocks = NULL;
    struct point *p_blocks = NULL;
    struct point *blocks = NULL;
    Q = calloc( 1, q * sizeof( unsigned char ) );

    if( Q == NULL ) {
        return -1;
    }

    memset( b0, 0, sizeof( b0 ) );
    clen_tlen_bytes = bits_2_bytes( Clen - Tlen );
    /* Generate Q */
    temp = clen_tlen_bytes;

    for( i = q; i > 0; i-- ) {
        Q[i - 1] = 0xff & temp;
        temp = temp >> 8;
    }

    hex_print( Q, q );
    /* Generate b0 */
    flag_B0 |= 0x00;

    if( a ) {
        flag_B0 |= 0x40;
    }

    flag_B0 |= ( t - 2 ) / 2 << 3;
    flag_B0 |= ( q - 1 );
    memcpy( b0, &flag_B0, 1 );
    memcpy( b0 + 1, N, n );
    memcpy( b0 + 1 + n, Q, q );
    free( Q );
    /* Format Associated Data*/
#ifdef AD_SENSITIVE

    if( a )
#endif
        format_associated_data( A, a, &a_blocks, &row_a );

    if( a_blocks == NULL ) {
        free( Q );
        return -1;
    }

    format_payload( P, clen_tlen_bytes, &p_blocks, &row_p );

    if( p_blocks == NULL ) {
        free( Q );
        destroy_struct( a_blocks, row_a );
        return -1;
    }

    *row = row_a + row_p + 1;
    blocks = calloc( 1, ( row_a + row_p + 1 ) * sizeof( struct point ) );

    if( blocks == NULL ) {
        free( Q );
        destroy_struct( a_blocks, row_a );
        destroy_struct( p_blocks, row_p );
        return -1;
    }

    blocks[0].a = calloc( 1, blockSize * sizeof( unsigned char ) );

    if( blocks[0].a == NULL ) {
        free( Q );
        destroy_struct( a_blocks, row_a );
        destroy_struct( p_blocks, row_p );
        free( blocks );
        return -1;
    }

    memcpy( blocks[0].a, b0, blockSize );

    for( j = 0; j < row_a; j++ ) {
        blocks[1 + j].a = calloc( 1, blockSize * sizeof( unsigned char ) );

        if( blocks[1 + j].a == NULL ) {
            free( Q );
            destroy_struct( a_blocks, row_a );
            destroy_struct( p_blocks, row_p );
            destroy_struct( blocks, 1 + j );
            return -1;
        }

        memcpy( blocks[1 + j].a, a_blocks[j].a, blockSize );
    }

    for( j = 0; j < row_p; j++ ) {
        blocks[1 + row_a + j].a = calloc( 1, blockSize * sizeof( unsigned char ) );

        if( blocks[1 + row_a + j].a == NULL ) {
            free( Q );
            destroy_struct( a_blocks, row_a );
            destroy_struct( p_blocks, row_p );
            destroy_struct( blocks, 1 + row_a + j );
            return -1;
        }

        memcpy( blocks[1 + row_a + j].a, p_blocks[j].a, blockSize );
    }

    destroy_struct( a_blocks, row_a );
    destroy_struct( p_blocks, row_p );
    *p = blocks;
    return 0;
}

/*!
 * You need to free the payload which returns not null
 * return NULL---INVALID  P---success
 */
unsigned char *decrypt( unsigned char *nonce, int n_len, unsigned char *associated, int a_len, unsigned char *ciphertext, int c_len, int *r_len, int t_len )
{
    int m;
    int row;
    int res;
    int i;
    int j;
    unsigned char Si[blockSize];
    unsigned char S0[blockSize];
    struct point *counters = NULL;
    unsigned char tmp_ret[blockSize]; /* for recv CIPH value */
    int clen_tlen_bytes;
    unsigned char *P = NULL;
    unsigned char *T = NULL; /* this */
    unsigned char *S_0 = NULL; /* this */
    struct point *blocks = NULL;
    unsigned char Yi[blockSize];
    unsigned char Y0[blockSize] ;
    unsigned char *Yr = NULL; /* this */
    memset( Yi, 0, sizeof( Yi ) );
    memset( Y0, 0, sizeof( Y0 ) );
    memset( tmp_ret, 0, sizeof( tmp_ret ) );
    memset( Si, 0, sizeof( Si ) );
    memset( S0, 0, sizeof( S0 ) );
    init_args( n_len, a_len, c_len, t_len );

    if( Clen <= Tlen ) {
        return NULL;
    }

    T = calloc( 1, t * sizeof( unsigned char ) );
    S_0 = calloc( 1, t * sizeof( unsigned char ) );
    Yr = calloc( 1, t * sizeof( unsigned char ) );

    if( !T || !S_0 || !Yr ) {
        if( T ) {
            free( T );
        }

        if( S_0 ) {
            free( S_0 );
        }

        if( Yr ) {
            free( Yr );
        }

        return NULL;
    }

    m = ( ( Clen - Tlen ) & 0x7f ) ? ( ( Clen - Tlen ) >> 7 ) + 1 : ( ( Clen - Tlen ) >> 7 );
    {
        unsigned char *S = calloc( 1, m * blockSize * sizeof( unsigned char ) );   /* this */

        if( S == NULL ) {
            free( T );
            free( S_0 );
            free( Yr );
            return NULL;
        }

        counter_generation( &counters, m + 1, nonce, n, flagCounter );

        if( counters == NULL ) {
            free( T );
            free( S_0 );
            free( Yr );
            free( S );
            return NULL;
        }

        /* For j=0 to m, do Sj= CIPHK(Ctrj) */
        for( j = 0; j <= m; j++ ) {
            memset( tmp_ret, 0, sizeof( tmp_ret ) );
            res = CIPH( tmp_ret, counters[j].a );

            if( res != 0 ) {
                free( T );
                free( S_0 );
                free( Yr );
                free( S );
                return NULL;
            }

            memcpy( Si, tmp_ret, blockSize );

            if( j == 0 ) {
                memcpy( S0, Si, blockSize );
            } else { /* Set S= S1 || S2 || ...|| Sm. */
                memcpy( S + ( j - 1 ) *blockSize, Si, blockSize );
            }
        }

        destroy_struct( counters, m + 1 );
        hex_print( S0, 16 );
        hex_print( S, m * 16 );
        /* Set P=MSBclen-Tlen(C) ^ MSBclen-Tlen(S) */
        clen_tlen_bytes = bits_2_bytes( Clen - Tlen );
        *r_len = clen_tlen_bytes;
        {
            unsigned char *S_t = calloc( 1, clen_tlen_bytes * sizeof( unsigned char ) );   //this
            //unsigned char P[clen_tlen_bytes]; /* here may not strict confirm the alrgim , because you just use Clen-Tlen bits */
            P = calloc( 1, clen_tlen_bytes * sizeof( unsigned char ) + 1 );

            if( P == NULL || S_t == NULL ) {
                free( S );
                free( T );
                free( S_0 );
                free( Yr );

                if( S_t ) {
                    free( S_t );
                }

                if( P ) {
                    free( P );
                }

                return NULL;
            }

            memcpy( P, ciphertext, clen_tlen_bytes );
            memcpy( S_t, S, clen_tlen_bytes );

            for( i = 0; i < clen_tlen_bytes; i++ ) {
                P[i] = P[i] ^ S_t[i];
            }

            hex_print( P, clen_tlen_bytes );
            free( S_t );
        }
        /* Set T=LSBTlen(C) ^ MSBTlen(S0) */
        memcpy( T, ciphertext + ( c - t ), t );
        hex_print( T, t );
        memcpy( S_0, S0, t );
        hex_print( S_0, t );

        for( i = 0; i < t; i++ ) {
            T[i] = T[i] ^ S_0[i];
        }

        hex_print( T, t );
        res = formatting( nonce, associated, P, &blocks, &row );

        if( res != 0 ) {
            free( S );
            free( T );
            free( S_0 );
            free( Yr );
            free( P );
            return NULL;
        }

        memset( tmp_ret, 0, sizeof( tmp_ret ) );
        res = CIPH( tmp_ret, blocks[0].a );

        if( res != 0 ) {
            free( S );
            free( T );
            free( S_0 );
            free( Yr );
            free( P );
            return NULL;
        }

        //memcpy(Y0, tmp_ret, 1);
        memcpy( Y0, tmp_ret, 16 );

        /* For i = 1 to r, do Yj = CIPHK(Bi ^ Yi-1) */
        for( i = 1; i < row; i++ ) {
            unsigned char tmp[blockSize];
            unsigned char ch;
            memset( tmp, 0, sizeof( tmp ) );
            memset( tmp_ret, 0, sizeof( tmp_ret ) );

            for( j = 0; j < blockSize; j++ ) {
                ch = * ( blocks[i].a + j ) ^ Yi[j];
                memcpy( tmp + j, &ch, 1 );
            }

            res = CIPH( tmp_ret, tmp );

            if( res != 0 ) {
                free( S );
                free( T );
                free( S_0 );
                free( Yr );
                free( P );
                return NULL;
            }

            memcpy( Yi, tmp_ret, blockSize );
        }

        destroy_struct( blocks, row );
        free( S );
        memcpy( Yr, Yi, t );
        hex_print( T, t );
        hex_print( Yr, t );
        free( T );
        free( S_0 );
        free( Yr );
        /*T!=MSBTlen(Yr)*/
        /*for(i =0; i< t; i++) {
          if (T[i] != Yr[i])
          break;}
         */
        tr_log( LOG_DEBUG, "decrypt result" );
        hex_print( P, clen_tlen_bytes );
        /*if (i != t)
          return NULL;
          else
         */
        * ( P + clen_tlen_bytes ) = '\0';
        return P;
    }
}

#endif //TR069_WIB
