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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pub.h"
#include "tr_lib.h"
#include "war_type.h"

#ifdef TR069_WIB
unsigned char *hmac_sha256( const void *key, int key_len, const char *msg, int msg_len, unsigned char md[SHA256_HASH_SIZE], unsigned int *md_len )
{
    SHA256_CTX ctx;
    unsigned char k_ipad[SHA256_BLOCK_SIZE];
    unsigned char k_opad[SHA256_BLOCK_SIZE];
    unsigned char tmp_key[SHA256_HASH_SIZE];
    int i;
    /* If key is longer than 64 bytes reset it to key=SHA1(key) length to be 20 bytes */

    if( key_len > SHA256_BLOCK_SIZE ) {
        SHA256_CTX tmp_ctx;
        SHA256_Init( &tmp_ctx );
        SHA256_Update( &tmp_ctx, ( unsigned char * ) key, key_len );
        //SHA256_Final(&tmp_ctx, tmp_key);
        SHA256_Final( tmp_key, &tmp_ctx );
        key = tmp_key;
        key_len = SHA256_HASH_SIZE;
    }

    memset( k_ipad, 0, sizeof( k_ipad ) );
    memset( k_opad, 0, sizeof( k_opad ) );
    memcpy( k_ipad, key, key_len );
    memcpy( k_opad, key, key_len );

    for( i = 0; i < SHA256_BLOCK_SIZE; i++ ) {
        k_ipad[i] ^= 0x36;
        k_opad[i] ^= 0x5c;
    }

    SHA256_Init( &ctx );
    SHA256_Update( &ctx, k_ipad, SHA256_BLOCK_SIZE );
    SHA256_Update( &ctx, ( unsigned char * ) msg, msg_len );
    SHA256_Final( md, &ctx );
    SHA256_Init( &ctx );
    SHA256_Update( &ctx, k_opad, SHA256_BLOCK_SIZE );
    SHA256_Update( &ctx, md, SHA256_HASH_SIZE );
    SHA256_Final( md, &ctx );
    *md_len = SHA256_HASH_SIZE;
    return md;
}

/*!
 * \fn get_bek
 * \brief Generate the BEK
 * \param aes_key Where the BEK store
 * \return 0 success -1 faild
 */
static int get_bek( unsigned char *aes_key )
{
    /* this is a demo
     * BEK = the 16 most significant (leftmost) octets of HMAC-SHA256(EMSK, "bek@wimaxforum.org")
     */
    char *emsk = NULL;
    int emsk_len;
    unsigned char md[SHA256_HASH_SIZE];
    unsigned int md_len;
    int key_len = 18;
    unsigned char key[] = {"bek@wimaxforum.org"};
    lib_get_emsk( &emsk );
    emsk_len = strlen( emsk );
    hmac_sha256( key, key_len, emsk, emsk_len, md, &md_len );
    memcpy( aes_key, md, 16 );
    free( emsk );
    hex_print( aes_key, 16 );
    return 0;
}

/*!
 * \fn CIPH
 * \brief Encrypt data using aes algorithm
 * \param out Where the result store
 * \param in Data which needs be encrypted
 * \return 0 success -1 failed
 */
int CIPH( unsigned char *out, unsigned char *in )
{
    int res;
    unsigned char aes_key[16];
    AES_KEY key;
    memset( aes_key, 0, sizeof( aes_key ) );
    res = get_bek( aes_key );

    if( res != 0 ) {
        return -1;
    }

    res = AES_set_encrypt_key( aes_key, 128, &key );

    if( res != 0 ) {
        return -1;
    }

    AES_encrypt( in, out, &key );
    return 0;
}
#endif //TR069_WIB

/*!
 * \fn __hex_print
 * \brief Convert decimalist to hex, and print to screen
 * \param d The decimalist number
 * \return none
 */

/* the next s_PPP ,p_PPP amd PPP is for test */
static void __hex_print( int d )
{
    char hex[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8',
                  '9', 'A', 'B', 'C', 'D', 'E', 'F'
                 };
    printf( "%c", hex[d] );
}

/*!
 * \fn _hex_print
 * \brief Convert unsigned char to hex, and print to screen
 * \param d The unsigned char number
 * \return none
 */
static void _hex_print( unsigned char d )
{
    int tmp ;
    tmp = ( d >> 4 ) & 0xf;
    __hex_print( tmp );
    tmp = d & 0x0f;
    __hex_print( tmp );
}

/*!
 * \fn hex_print
 * \brief Convert unsigned char to hex, and print to screen
 * \param input The unsigned char data
 * \param input_bytes The length of unsigned char, which need to print
 * \return none
 */
void hex_print( unsigned char *input, int input_bytes )
{
    int i;

    for( i = 0; i < input_bytes; i++ ) {
        _hex_print( input[i] );
    }
}

/*!
 * \fn bits_2_bytes
 * \brief Convert bits to bytes(top integral)
 * \param bits The bits
 * \return The top integral
 */
int bits_2_bytes( int bits )
{
    int bytes = ( bits & 0x07 ) ? ( ( bits ) >> 3 ) + 1 : ( bits ) >> 3;
    return bytes;
}

/*!
 * \fn destroy_struct
 * \brief Destroy struct pointer
 * \param p The pointer of struct need to be destroyed
 * \param r How many structs need to be destroyed
 * \return none
 */
void destroy_struct( struct point *p, int r )
{
    int i;

    for( i = 0; i < r; i++ ) {
        if( p[i].a ) {
            free( p[i].a );
        }
    }

    if( p ) {
        free( p );
        p = NULL;
    }
}

/*!
 * \fn cover_num_2_bytes
 * \brief Covert decimal to bytes
 * \param *p The pointer where stores result
 * \param num The decimal number
 * \param bytes How many bytes to convert
 * \return None
 */
void conver_num_2_bytes( unsigned char **p, int num, int bytes )
{
    int i;
    int temp = num;
    unsigned char *tmp = NULL;
    tmp = calloc( 1, bytes * sizeof( unsigned char ) );

    if( tmp == NULL ) {
        *p = NULL;
    } else {
        for( i = bytes; i > 0; i-- ) {
            tmp[i - 1] = 0xff & temp;
            temp = temp >> 8;
        }

        *p = tmp;
    }
}

/*!
 * \fn alen_2_encodelen
 * \brief Computer the encode length according to the length of associated data
 * \param Alen: the associate length
 * \Note:  If 0 < a < (2)16-(2)8, then a is encoded as [a]16, i.e., two octets.
 * \Note:  If (2)16-(2)8 <= a < (2)32, then a is encoded as 0xff || 0xfe || [a]32, i.e., six octets.
 * \Note:  If (2)32 <= a < (2)64, then a is encoded as 0xff || 0xff || [a]64, i.e., ten octets
 * \return Endode length
 */
int alen_2_encodelen( int Alen )
{
    int encode_len;
    /*if (Alen > 0 && Alen < ADATA_level1)
        encode_len = 2;//[a]16
    else if (Alen >= ADATA_level1 && Alen < ADATA_level2)
        encode_len = 4;//0xff || 0xfe || [a]32
    else
        encode_len = 8;//0xff || 0xff || [a]6*/
    encode_len = 2; /* [a]16 */
    return encode_len;
}

/*!
 * \fn argscat
 * \brief Cat args
 * \param dec The pointer where stores result
 * \param arg1 The first argument
 * \param arg1_len Length of the first argument
 * \param arg2 Tthe second argument
 * \param arg2_len Length of the second argument
 * \return None
 */
void argscat( unsigned char **dec, unsigned char *arg1, int arg1_len, unsigned char *arg2, int arg2_len )
{
    unsigned char *tmp = NULL;
    tmp = calloc( 1, ( arg1_len + arg2_len ) * sizeof( unsigned char ) );

    if( tmp == NULL ) {
        *dec = NULL;
    } else {
        memcpy( tmp, arg1, arg1_len );
        memcpy( tmp + arg1_len, arg2, arg2_len );
        *dec = tmp;
    }
}

/*
 * \brief The counter generation function to generate the counter blocks
 */
void counter_generation( struct point **p, int m, unsigned char *N, int n_len, unsigned char flag_counter )
{
    int i;
    int error = 0;
    struct point *counters = calloc( 1, ( m ) * sizeof( struct point ) );

    if( counters == NULL ) {
        *p = NULL;
    } else {
        for( i = 0; i < m; i++ ) {
            int j;
            int tmp = i;
            counters[i].a = calloc( 1, blockSize * sizeof( unsigned char ) );

            if( counters[i].a == NULL ) {
                destroy_struct( counters, i );
                error = 1;
                break;
            } else {
                memcpy( counters[i].a, &flag_counter, 1 );
                memcpy( counters[i].a + 1, N, n_len ); //1--n bytes

                for( j = 15 ; j > n_len; j-- ) { //n--15 bytes
                    * ( counters[i].a + j ) = 0xff & tmp;
                    tmp = tmp >> 8;
                }
            }
        }

        if( error ) {
            *p = NULL;
        } else {
            *p = counters;
        }
    }
}

/*
 * \brief Formatting function to (N, A, P) to produce the blocks B0, B1, ¡K, Br
 */
void format_payload( unsigned char *P, int p_len, struct point **r, int *row )
{
    int size;
    int i, error = 0;
    struct point *p_blocks = NULL;
    size = ( p_len ) / blockSize;

    if( p_len % blockSize ) {
        size++;
    }

    *row = size;
    p_blocks = calloc( 1, size * sizeof( struct point ) );

    if( p_blocks == NULL ) {
        *r = NULL;
    } else {
        for( i = 0; i < size; i++ ) {
            p_blocks[i].a = calloc( 1, blockSize * sizeof( unsigned char ) );

            if( p_blocks[i].a == NULL ) {
                destroy_struct( p_blocks, i );
                error = 1;
                break;
            } else {
                memcpy( p_blocks[i].a, P + i * blockSize, i == size - 1 ? p_len % blockSize : blockSize );
            }
        }

        if( error ) {
            *r = NULL;
        } else {
            *r = p_blocks;
        }
    }
}

void format_associated_data( unsigned char *A, int a_len, struct point **p, int *row )
{
    int i;
    int size;
    int error = 0;
    int encode_len;
    struct point *a_blocks = NULL;
    unsigned char *result_t = NULL; /* Free */
    unsigned char *result = NULL; /* Result free */
    unsigned char *result_fin = NULL; /* Free */
    encode_len = alen_2_encodelen( a_len );
    conver_num_2_bytes( &result_t, a_len, encode_len );

    if( result_t == NULL ) {
        *p = NULL;
    } else {
        argscat( &result, result_t, encode_len, A, a_len );
        free( result_t );

        if( result == NULL ) {
            *p = NULL;
        } else {
            size = ( encode_len + a_len ) / blockSize;

            if( ( encode_len + a_len ) % blockSize ) {
                size++;
            }

            *row = size;
            result_fin = calloc( 1, size * blockSize * sizeof( unsigned char ) );

            if( result_fin == NULL ) {
                *p = NULL;
            } else {
                memcpy( result_fin, result, encode_len + a_len );
                a_blocks = calloc( 1, size * sizeof( struct point ) );

                if( a_blocks == NULL ) {
                    *p = NULL;
                } else {
                    for( i = 0; i < size; i++ ) {
                        a_blocks[i].a = calloc( 1, blockSize * sizeof( unsigned char ) );

                        if( a_blocks[i].a == NULL ) {
                            destroy_struct( a_blocks, i );
                            error = 1;
                        } else {
                            memcpy( a_blocks[i].a, result_fin + i * blockSize, blockSize );
                        }
                    }

                    if( error ) {
                        *p = NULL;
                    } else {
                        *p = a_blocks;
                    }
                }

                free( result_fin );
            }

            free( result );  /* Free result */
        }
    }
}

