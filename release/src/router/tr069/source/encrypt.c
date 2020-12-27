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
#ifdef WKS_EXT

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "b64.h"
#include "tr_lib.h"


/* Calculate Content Encryption Keys  0 -1 */
int calculate_cek( char *s_cookie, unsigned char *cek )
{
    int res = -1;
    MD5_CTX ctx;
    unsigned char final[MD5_DIGEST_LENGTH];
    char *sn = NULL, *key = NULL;
    node_t node_s, node_k;
    res = lib_resolve_node( SERIALNUMBER, &node_s );
    res |= lib_resolve_node( X_WKS_KEY, &node_k );

    if( res == 0 ) {
        res = lib_get_value( node_s, &sn );
        res |= lib_get_value( node_k, &key );

        if( res == 0 ) {
            //char *s_cookie = "E8DBDA71BB48B285B311723858896093";// joyce test admin ag0IYZiMgwnfBzkQz0rkYg==
            //MD5([ACS-provided]_[CPE-provided]_[pre-shared])
            MD5_Init( &ctx );
            MD5_Update( &ctx, s_cookie, strlen( s_cookie ) );
            MD5_Update( &ctx, "_", 1 );
            MD5_Update( &ctx, sn, strlen( sn ) );
            MD5_Update( &ctx, "_", 1 );
            MD5_Update( &ctx, key, strlen( key ) );
            MD5_Final( final, &ctx );
            memcpy( cek, final, 16 );
        }

        if( sn ) {
            free( sn );
        }

        if( key ) {
            free( key );
        }
    }

    return res;
}

#define BLOCKSIZE  16

/* Decrypt encrypted value by cek
 * ciphertext = B(AES(original))
 */
int content_decrypt( char *value, unsigned char *cek, int cek_len, unsigned char **output )
{
    int res;
    AES_KEY key;
    unsigned char en_out[16];
    unsigned char en_in[16];
    int input_len;
    int blocks;
    int i;
    unsigned char *g_in = NULL;
    unsigned char *g_out = NULL;
    input_len = strlen( value );
    /*
       char *cur = NULL, *next = NULL;
       for (cur = value + input_len; cur; cur--) {
       if (*cur != '=') {
       break;
       }
       redundancy++;
       }
       g_in = calloc(1, input_len/4*3 - redundancy);
     */
    blocks = ( input_len / 4 * 3 ) / 16;
    g_in = calloc( 1, blocks * BLOCKSIZE + 4 );
    g_out = calloc( 1, blocks * BLOCKSIZE + 4 );

    if( g_in == NULL || g_out == NULL ) {
        if( g_in ) {
            free( g_in );
        }

        if( g_out ) {
            free( g_out );
        }

        return -1;
    }

    res = b64_decode( value, input_len, g_in, blocks * BLOCKSIZE + 1 );

    if( res != ( blocks * 16 ) ) {
        tr_log( LOG_ERROR, "b64 decode faild" );
        free( g_in );
        free( g_out );
        return -1;
    }

    for( i = 0; i < blocks; i++ ) {
        memset( en_out, 0, sizeof( en_out ) );
        memset( en_in, 0, sizeof( en_in ) );
        memcpy( en_in, g_in + BLOCKSIZE * i, BLOCKSIZE );
        AES_set_decrypt_key( cek, cek_len * 8, &key ) ;
        AES_decrypt( en_in, en_out, &key );
        memcpy( g_out + i * BLOCKSIZE, en_out, BLOCKSIZE );
    }

    if( g_in ) {
        free( g_in );
    }

    *output = g_out;
    return 0;
}

int content_encrypt( char *value, unsigned char *cek, int cek_len, char **output )
{
    AES_KEY key;
    char *out = NULL;
    int out_len;
    int input_len;
    int blocks ;
    int i;
    unsigned char en_in[16];
    unsigned char en_out[16];
    unsigned char *g_in;
    unsigned char *g_out;
    input_len = strlen( value );
    blocks = ( input_len % 16 ) ? input_len / 16 + 1 : input_len / 16;
    g_in = calloc( 1, blocks * BLOCKSIZE );
    g_out = calloc( 1, blocks * BLOCKSIZE );
    memset( g_in, 0, blocks * BLOCKSIZE );
    memset( g_out, 0, blocks * BLOCKSIZE );

    for( i = 0; i < input_len; i++ ) {
        g_in[i] = value[i];
    }

    printf( "\n" );

    for( i = 0; i < blocks; i++ ) {
        printf( "block%d\n", i );
        memset( en_out, 0, sizeof( en_out ) );
        memcpy( en_in, g_in + i * 16, 16 );
        AES_set_encrypt_key( cek, cek_len * 8, &key );
        AES_encrypt( en_in, en_out, &key );
        printf( "block%d encrypt\n", i );
        memcpy( g_out + i * BLOCKSIZE, en_out, BLOCKSIZE );
    }

    printf( "\n" );
    out_len = ( BLOCKSIZE * blocks + 2 ) / 3 * 4 + 1;
    out = calloc( 1, out_len );
    b64_encode( g_out, BLOCKSIZE * blocks, out, out_len );

    if( g_in ) {
        free( g_in );
    }

    if( g_out ) {
        free( g_out );
    }

    *output = out;
    return 0;
}

#endif //WKS_EXT
