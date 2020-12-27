/*
 * Radius support for NAS workspace
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 * $Id: nas_wksp_radius.c 437585 2013-11-19 10:49:52Z $
 */

#include <typedefs.h>
#include <assert.h>
#include <unistd.h>
#include <proto/eap.h>
#include <errno.h>

#include "nas.h"
#include "nas_wksp.h"
#include "nas_radius.h"
#include "nas_wksp_radius.h"

#ifdef NAS_WKSP_BUILD_NAS_AUTH
/*
* Common NAS communication routines that can be used under different
* OSs. These functions need to be re-implemented only when the socket
* layer interface provided for the specific operating system is not
* the similiar kind to that in this implementation.
*/
#define NAS_WKSP_RADIUS_TRIES	5
#define NAS_WKSP_RADIUS_SLEEP	5	/* in seconds */

#ifdef NAS_IPV6
#define NS_INT16SZ       2
#define NS_INADDRSZ     4
#define NS_IN6ADDRSZ    16


int
inet_ntop4(const unsigned char *src, char *dst, size_t size) {
    static const char *fmt = "%u.%u.%u.%u";
    char tmp[sizeof "255.255.255.255"];

    if ( (size_t)sprintf(tmp, fmt, src[0], src[1], src[2], src[3]) >= size ) {
        return 0;
    }
    strcpy(dst, tmp);

    return 1;
}

/* const char *
 * isc_inet_ntop6(src, dst, size)
 *      convert IPv6 binary address into presentation (printable) format
 * author:
 *      Paul Vixie, 1996.
 */
int
inet_ntop6(const unsigned char *src, char *dst, size_t size) {
    /*
     * Note that int32_t and int16_t need only be "at least" large enough
     * to contain a value of the specified size.  On some systems, like
     * Crays, there is no such thing as an integer variable with 16 bits.
     * Keep this in mind if you think this function should have been coded
     * to use pointer overlays.  All the world's not a VAX.
     */
    char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"], *tp;
    struct {
        int base, len;
    } best, cur;
    unsigned int words[NS_IN6ADDRSZ / NS_INT16SZ];
    int i;

    /*
     * Preprocess:
     *      Copy the input (bytewise) array into a wordwise array.
     *      Find the longest run of 0x00's in src[] for :: shorthanding.
     */
    memset(words, '\0', sizeof words);
    for ( i = 0; i < NS_IN6ADDRSZ; i++ )
        words[i / 2] |= (src[i] << ((1 - (i % 2)) << 3));
    best.base = -1;
    best.len = 0;
    cur.base = -1;
    cur.len = 0;
    for ( i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++ ) {
        if ( words[i] == 0 ) {
            if ( cur.base == -1 )
                cur.base = i, cur.len = 1;
            else
                cur.len++;
        } else {
            if ( cur.base != -1 ) {
                if ( best.base == -1 || cur.len > best.len )
                    best = cur;
                cur.base = -1;
            }
        }
    }
    if ( cur.base != -1 ) {
        if ( best.base == -1 || cur.len > best.len )
            best = cur;
    }
    if ( best.base != -1 && best.len < 2 )
        best.base = -1;

    /*
     * Format the result.
     */
    tp = tmp;
    for ( i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++ ) {
        /* Are we inside the best run of 0x00's? */
        if ( best.base != -1 && i >= best.base &&
             i < (best.base + best.len) ) {
            if ( i == best.base )
                *tp++ = ':';
            continue;
        }
        /* Are we following an initial run of 0x00s or any real hex? */
        if ( i != 0 )
            *tp++ = ':';
        /* Is this address an encapsulated IPv4? */
        if ( i == 6 && best.base == 0 &&
             (best.len == 6 || (best.len == 5 && words[5] == 0xffff)) ) {
            if ( !inet_ntop4(src+12, tp,
                             sizeof tmp - (tp - tmp)) )
                return 0;
            tp += strlen(tp);
            break;
        }
        tp += sprintf(tp, "%x", words[i]);
    }
    /* Was it a trailing run of 0x00's? */
    if ( best.base != -1 && (best.base + best.len) ==
         (NS_IN6ADDRSZ / NS_INT16SZ) )
        *tp++ = ':';
    *tp++ = '\0';

    /*
     * Check for overflow, copy, and we're done.
     */
    if ( (size_t)(tp - tmp) > size ) {
        errno = ENOSPC;
        return 0;
    }
    strcpy(dst, tmp);
    return 1;
}
#endif

/* establish connection to radius server */
int
nas_radius_open(nas_wksp_t *nwksp, nas_wpa_cb_t *nwcb)
{
	nas_t *nas = &nwcb->nas;
	int n, i;

	/*
	* Prevent a descriptor leak in case the connection was broken by
	* the server and some one tries to re-establish the connection
	* with the server.
	*/
	if (nas->wan != NAS_WKSP_UNK_FILE_DESC) {
		NASDBG("%s: close radius socket %d\n", nas->interface, nas->wan);
		close(nas->wan);
		nas->wan = NAS_WKSP_UNK_FILE_DESC;
	}

	/* Connect to server */
#ifdef NAS_IPV6
	if ((nas->wan = socket(((struct sockaddr_in *)&(nas->server))->sin_family, SOCK_DGRAM, IPPROTO_UDP)) < 0)
#else
	if ((nas->wan = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
#endif
        {
		NASDBG("%s: Unable to create radius socket\n", nas->interface);
		goto exit0;
	}

	/* Give the RADIUS server a little time in case it's on
	 * the WAN.  It could be opened on demand later, but it's
	 * good to get an open descriptor now if possible.
	 */
	for (i = 0; i < NAS_WKSP_RADIUS_TRIES; i ++) {
#ifdef NAS_IPV6
        if (connect(nas->wan, (struct sockaddr *)&nas->server,
            (((struct sockaddr_in *)&(nas->server))->sin_family == AF_INET6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in))) < 0)
#else
		if (connect(nas->wan, (struct sockaddr *)&nas->server, sizeof(nas->server)) < 0)
#endif
        {
			NASDBG("%s: Unable to connect radius socket %d\n", nas->interface,
			       nas->wan);
#ifdef NAS_IPV6
            if (((struct sockaddr_in *)&(nas->server))->sin_family == AF_INET6 )
            {
                char tmp[200];
                if(inet_ntop6((unsigned char *)&(((struct sockaddr_in6 *)(&(nas->server)))->sin6_addr), tmp, 200))
                    NASDBG("%s: ipv6=%s\n", nas->interface, tmp);
                else
                    NASDBG("%s: transfer ipv6 addr to str fail\n", nas->interface);
            }
            else
            {
                char tmp[200];
                if(inet_ntop4((unsigned char *)&(((struct sockaddr_in *)(&(nas->server)))->sin_addr), tmp, 200))
                    NASDBG("%s: ipv4=%s\n", nas->interface, tmp);
                else
                    NASDBG("%s: transfer ipv4 addr to str fail\n", nas->interface);
            }
#endif

			nas_sleep_ms(NAS_WKSP_RADIUS_SLEEP*1000);
			continue;
		}
		n = sizeof(nas->client);
		if (getsockname(nas->wan, (struct sockaddr *)&nas->client, (socklen_t *)&n) < 0)
			break;
		NASDBG("%s: opened radius socket %d\n", nas->interface, nas->wan);
		return 0;
	}

	/* It should never come to here - error! */
	close(nas->wan);
exit0:
	NASDBG("%s: failed to open radius socket\n", nas->interface);
	nas->wan = NAS_WKSP_UNK_FILE_DESC;
	return errno;
}

/* close the connection to radius server */
void
nas_radius_close(nas_wksp_t *nwksp, nas_wpa_cb_t *nwcb)
{
	nas_t *nas = &nwcb->nas;
	if (nas->wan != NAS_WKSP_UNK_FILE_DESC) {
		NASDBG("%s: close radius socket %d\n", nas->interface, nas->wan);
		close(nas->wan);
		nas->wan = NAS_WKSP_UNK_FILE_DESC;
	}
}

/* establish connection to radius server for each interface */
int
nas_wksp_open_radius(nas_wksp_t *nwksp)
{
	nas_wpa_cb_t *nwcb;
	int i;

	for (i = 0; i < nwksp->nwcbs; i ++) {
		nwcb = nwksp->nwcb[i];
		assert(nwcb);
		/* open connection to radius server */
		if (CHECK_RADIUS(nwcb->nas.mode)) {
				/* open connection to radius server */
				nas_radius_open(nwksp, nwcb);
		}
	}
	return 0;
}

/* close connection to radius server for each interface */
void
nas_wksp_close_radius(nas_wksp_t *nwksp)
{
	nas_wpa_cb_t *nwcb;
	int i;

	for (i = 0; i < nwksp->nwcbs; i ++) {
		nwcb = nwksp->nwcb[i];
		assert(nwcb);
		if (CHECK_RADIUS(nwcb->nas.mode)) {
			nas_radius_close(nwksp, nwcb);
		}
	}
}

/* send radius packet to radius server */
int
nas_radius_send_packet(nas_t *nas, radius_header_t *radius, int length)
{
	int ret;
	nas_wpa_cb_t *nwcb = (nas_wpa_cb_t *)nas->appl;
	NASDBG("%s: sending packet to radius socket %d\n", nas->interface, nas->wan);
	if ((ret = send(nas->wan, (char *)radius, length, 0)) < 0) {
		NASDBG("%s: send error %d to radius socket %d\n", nas->interface, errno, nas->wan);
		/* Try re-opening it once before giving up. */
		/* This could happen if the peer has been reset */
		if (errno == EBADF) {
			if (!nas_radius_open(nwcb->nwksp, nwcb)) {
				NASDBG("%s: resending packet to radius socket %d\n", nas->interface,
				       nas->wan);
				ret = send(nas->wan, (char *)radius, length, 0);
				if (ret < 0)
					NASDBG("%s: resend error %d to radius socket %d\n",
					       nas->interface, errno, nas->wan);
			}
		}
	}
	return ret;
}

#endif	/* #ifdef NAS_WKSP_BUILD_NAS_AUTH */
