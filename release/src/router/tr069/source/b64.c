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
 * \file b64.h
 *
 * \brief The base64 algorithm(only need the encode) implementation
 */
#include "b64.h"
#include "log.h"

int b64_encode( const unsigned char *input, int input_len, char *output, int output_len )
{
    static const char b64_alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int inlen;
    int outlen;
    int mod;

    if( ( input_len + 2 ) / 3 * 4 + 1 > output_len ) {
        /* Make sure the output buffer length is enough */
        tr_log( LOG_WARNING, "Out put buffer is not enough!" );
        return -1;
    }

    mod = input_len % 3;
    input_len -= mod;

    for( inlen = 0, outlen = 0; input_len > inlen; ) {
        output[outlen++] = b64_alpha[input[inlen] >> 2];
        output[outlen++] = b64_alpha[( ( input[inlen] & 0x03 ) << 4 ) | ( input[inlen + 1] >> 4 ) ];
        inlen++;
        output[outlen++] = b64_alpha[( ( input[inlen] & 0x0f ) << 2 ) | ( input[inlen + 1] >> 6 ) ];
        output[outlen++] = b64_alpha[input[++inlen] & 0x3f];
        inlen++;
    }

    if( mod ) {
        output[outlen++] = b64_alpha[input[inlen] >> 2];
        output[outlen++] = b64_alpha[( ( input[inlen] & 0x03 ) << 4 ) | ( mod > 1 ? ( input[inlen + 1] >> 4 ) : 0 ) ];

        if( mod == 2 ) {
            output[outlen++] = b64_alpha[( input[inlen + 1] & 0x0f ) << 2];
        } else {
            output[outlen++] = '=';
        }

        output[outlen++] = '=';
    }

    output[outlen] = '\0';
    return 0;
}

int b64_decode( const char *src, int src_len, unsigned char *dst, int dst_len )
{
    int i, j;
    int padding = 0;
    unsigned char base64_decode_map[256] = {
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 62, 255, 255, 255, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 255, 255,
        255, 0, 255, 255, 255, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 255, 255, 255, 255, 255, 255, 26, 27, 28,
        29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
        49, 50, 51, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
    };

    if( src_len % 4 != 0 ) {
        return -1;
    }

    if( src_len > 2 && src[src_len - 1] == '=' ) {
        padding++;

        if( src[src_len - 2] == '=' ) {
            padding++;
        }
    }

    if( src_len / 4 * 3 - padding + 1 > dst_len ) {
        /* Make sure the least data is enough */
        return -1;
    }

    for( i = j = 0 ; i < src_len; i += 4 ) {
        dst[j++] = base64_decode_map[( int ) src[i]] << 2 |
                   base64_decode_map[( int ) src[i + 1]] >> 4;
        dst[j++] = base64_decode_map[( int ) src[i + 1]] << 4 |
                   base64_decode_map[( int ) src[i + 2]] >> 2;
        dst[j++] = base64_decode_map[( int ) src[i + 2]] << 6 |
                   base64_decode_map[( int ) src[i + 3]];
    }

    dst[j] = '\0';
    return j - padding;
}



int string_is_b64( const char *str )
{
    const char *s;
    s = str;

    if( *s == '\0' ) {
        return 1;
    }

    while( *s ) {
        if( ( *s >= 'A' && *s <= 'Z' ) || ( *s >= 'a' && *s <= 'z' ) || ( *s >= '0' && *s <= '9' ) || *s == '+' || *s == '/' ) {
            s++;
        } else if( *s == '=' && ( ( ( ( int )( s - str ) ) % 4 ) == 2 || ( ( ( int )( s - str ) ) % 4 ) == 3 ) ) {
            const char b64_alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            int index;

            for( index = 0; b64_alpha[index]; index++ ) {
                if( b64_alpha[index] == s[-1] ) {
                    break;
                }
            }

            if( ( ( int )( s - str ) ) % 4 == 2 ) {
                if( ( index & 0xCF ) == 0 && s[1] == '=' && s[2] == '\0' ) {
                    return 1;
                } else {
                    return 0;
                }
            } else {
                if( ( index & 0xC3 ) == 0 && s[1] == '\0' ) {
                    return 1;
                } else {
                    return 0;
                }
            }
        } else {
            break;
        }
    }

    return 0;
}

#ifdef UNIT_TEST
#include <assert.h>

static void __attribute__( ( constructor ) ) __test_b64()
{
    struct {
        char *plain;
        char *b64;
    } data[] = {
        {"", ""},
        {"123456", "MTIzNDU2"},
        {"1234567", "MTIzNDU2Nw=="},
        {"12345678", "MTIzNDU2Nzg="}
    };
    char output[32];
    int i;

    for( i = 0; i < sizeof( data ) / sizeof( data ); i++ ) {
        assert( b64_encode( ( unsigned char * )( data[i].plain ), strlen( data[i].plain ), output, sizeof( output ) ) == 0 );
        assert( strcmp( output, data[i].b64 ) == 0 );
        //Ouput space not engough
        assert( b64_encode( ( unsigned char * )( data[i].plain ), strlen( data[i].plain ), output, strlen( data[i].b64 ) ) == -1 );
        assert( b64_decode( data[i].b64, strlen( data[i].b64 ), ( unsigned char * )output, sizeof( output ) ) == 0 );
        assert( strcmp( output, data[i].plain ) == 0 );
        //Ouput space not engough
        assert( b64_encode( ( unsigned char * )( data[i].b64 ), strlen( data[i].b64 ), output, strlen( data[i].plain ) ) == -1 );
        assert( string_is_b64( data[i].b64 ) == 1 );
    }

    //test two illegal base64 strings
    assert( string_is_b64( "MTIzNDU2NzB=" ) == 0 );
    assert( string_is_b64( "MTIzNDU2Nz=" ) == 0 );
}

#endif
