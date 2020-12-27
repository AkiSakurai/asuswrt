/*
 * Copyright(c) 2013, ASUSTeK Inc.
 * All rights reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of ASUSTeK Inc.;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <tr_lib.h>
#include "utils.h"
#include "device.h"

#include <shared.h>
#include <shutils.h>
#ifdef ASUSWRT
#include <bcmnvram.h>
#ifdef RTCONFIG_QTN
#include "web-qtn.h"
#endif
#else 	/* DSL_ASUSWRT */
#include "libtcapi.h"
#include <tcutils.h> 
#endif

/* for icmp */
#include <sys/time.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/ether.h>
#ifndef RTCONFIG_QTN
#include <netpacket/packet.h>
#endif
#include <netdb.h>
#include <netinet/ip_icmp.h>

/*
* parse ifname to retrieve unit #
*/
#if 0
int ppp_ifunit(char *ifname)
{
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";

	for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit++) {
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		if (nvram_match(strcat_r(prefix, "pppoe_ifname", tmp), ifname))
			return unit;
	}

	return -1;
}

int wan_ifunit(char *wan_ifname)
{
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	
	if ((unit = ppp_ifunit(wan_ifname)) >= 0) {
		return unit;
	} else {
		for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit++) {
			snprintf(prefix, sizeof(prefix), "wan%d_", unit);
			if (nvram_match(strcat_r(prefix, "ifname", tmp), wan_ifname) &&
			    (nvram_match(strcat_r(prefix, "proto", tmp), "dhcp") ||
			     nvram_match(strcat_r(prefix, "proto", tmp), "static")))
				return unit;
		}
	}

	return -1;
}
#endif

/* +++ MD5 +++ */
typedef struct {
	unsigned long state[4];			/* state (ABCD) */
	unsigned long count[2];			/* number of bits, modulo 2^64 (lsb first) */
 	unsigned char buffer[64];		/* input buffer */
} ASUS_MD5_CTX;

#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

static void ASUS_MD5Transform(unsigned long [4], unsigned char [64]);
static void ASUS_Encode(unsigned char *, unsigned long *, unsigned int);
static void ASUS_Decode(unsigned long *, unsigned char *, unsigned int);
static unsigned char PADDING[64] = {
  0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* F, G, H and I are basic MD5 functions.
 */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits.
 */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
Rotation is separate from addition to prevent recomputation.
 */
#define FF(a, b, c, d, x, s, ac) { \
 (a) += F ((b), (c), (d)) + (x) + (unsigned long)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define GG(a, b, c, d, x, s, ac) { \
 (a) += G ((b), (c), (d)) + (x) + (unsigned long)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define HH(a, b, c, d, x, s, ac) { \
 (a) += H ((b), (c), (d)) + (x) + (unsigned long)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define II(a, b, c, d, x, s, ac) { \
 (a) += I ((b), (c), (d)) + (x) + (unsigned long)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }

/* MD5 initialization. Begins an MD5 operation, writing a new context.
 */
void ASUS_MD5Init (ASUS_MD5_CTX * context)
{
	context->count[0] = context->count[1] = 0;
	/* Load magic initialization constants.*/
	context->state[0] = 0x67452301;
	context->state[1] = 0xefcdab89;
	context->state[2] = 0x98badcfe;
	context->state[3] = 0x10325476;
}

/* MD5 block update operation. Continues an MD5 message-digest
  operation, processing another message block, and updating the
  context.
 */
void ASUS_MD5Update (ASUS_MD5_CTX *context, unsigned char *input, unsigned int inputLen)
{
	unsigned int i, index, partLen;

  	/* Compute number of bytes mod 64 */
  	index = (unsigned int)((context->count[0] >> 3) & 0x3F);

	/* Update number of bits */
	if ((context->count[0] += ((unsigned long)inputLen << 3)) < ((unsigned long)inputLen << 3))
		context->count[1]++;
  	context->count[1] += ((unsigned long)inputLen >> 29);

  	partLen = 64 - index;

  /* Transform as many times as possible. */
  	if (inputLen >= partLen) {
 		memcpy(&context->buffer[index], input, partLen);
 		ASUS_MD5Transform (context->state, context->buffer);

 		for (i = partLen; i + 63 < inputLen; i += 64)
   			ASUS_MD5Transform (context->state, &input[i]);

		index = 0;
  	}
  	else
 		i = 0;

  /* Buffer remaining input */
  	memcpy(&context->buffer[index], &input[i],inputLen-i);
}


/* MD5 finalization. Ends an MD5 message-digest operation, writing the
  the message digest and zeroizing the context.
 */
void ASUS_MD5Final (unsigned char digest[16], ASUS_MD5_CTX *context)
{
  unsigned char bits[8];
  unsigned int index, padLen;

  /* Save number of bits */
  ASUS_Encode (bits, context->count, 8);

  /* Pad out to 56 mod 64.
*/
  index = (unsigned int)((context->count[0] >> 3) & 0x3f);
  padLen = (index < 56) ? (56 - index) : (120 - index);
  ASUS_MD5Update (context, PADDING, padLen);

  /* Append length (before padding) */
  ASUS_MD5Update (context, bits, 8);

  /* Store state in digest */
  ASUS_Encode (digest, context->state, 16);

  /* Zeroize sensitive information.
*/
  memset (context, 0, sizeof (*context));
}

/* MD5 basic transformation. Transforms state based on block.
 */
static void ASUS_MD5Transform (unsigned long state[4], unsigned char block[64])
{
  unsigned long a = state[0], b = state[1], c = state[2], d = state[3], x[16];

  ASUS_Decode (x, block, 64);

  /* Round 1 */
  FF (a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
  FF (d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
  FF (c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
  FF (b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
  FF (a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
  FF (d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
  FF (c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
  FF (b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
  FF (a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
  FF (d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
  FF (c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
  FF (b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
  FF (a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
  FF (d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
  FF (c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
  FF (b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

 /* Round 2 */
  GG (a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
  GG (d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
  GG (c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
  GG (b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
  GG (a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
  GG (d, a, b, c, x[10], S22,  0x2441453); /* 22 */
  GG (c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
  GG (b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
  GG (a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
  GG (d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
  GG (c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
  GG (b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
  GG (a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
  GG (d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
  GG (c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
  GG (b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

  /* Round 3 */
  HH (a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
  HH (d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
  HH (c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
  HH (b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
  HH (a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
  HH (d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
  HH (c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
  HH (b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
  HH (a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
  HH (d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
  HH (c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
  HH (b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
  HH (a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
  HH (d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
  HH (c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
  HH (b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */

  /* Round 4 */
  II (a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
  II (d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
  II (c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
  II (b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
  II (a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
  II (d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
  II (c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
  II (b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
  II (a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
  II (d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
  II (c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
  II (b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
  II (a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
  II (d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
  II (c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
  II (b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;

  /* Zeroize sensitive information.
*/
  memset (x, 0, sizeof (x));
}

/* Encodes input (unsigned long) into output (unsigned char). Assumes len is
  a multiple of 4.
 */
static void ASUS_Encode (unsigned char *output, unsigned long *input, unsigned int len)
{
	unsigned int i, j;

	for (i = 0, j = 0; j < len; i++, j += 4) {
		output[j] = (unsigned char)(input[i] & 0xff);
		output[j+1] = (unsigned char)((input[i] >> 8) & 0xff);
		output[j+2] = (unsigned char)((input[i] >> 16) & 0xff);
		output[j+3] = (unsigned char)((input[i] >> 24) & 0xff);
	}
}

/* Decodes input (unsigned char) into output (unsigned long). Assumes len is
  a multiple of 4.
 */
static void ASUS_Decode (unsigned long *output, unsigned char *input, unsigned int len)
{
	unsigned int i, j;

	for (i = 0, j = 0; j < len; i++, j += 4)
		output[i] = ((unsigned long)input[j]) | (((unsigned long)input[j+1]) << 8) |
		(((unsigned long)input[j+2]) << 16) | (((unsigned long)input[j+3]) << 24);
}
/* --- MD5 --- */

#define SPRINT_MAX_LEN	128
void generate_wep_key(int unit, int subunit, char *passphrase)
{
	int  i, j, wPasswdLen, randNumber;
	char UserInputKey[127];
	unsigned char pseed[4]={0,0,0,0};
	unsigned int wep_key[4][5];
	static char string[SPRINT_MAX_LEN];
	char tmp[32], prefix[] = "wlXXXXXXXXXX_";
	static char tmpstr[SPRINT_MAX_LEN];
	static long tmpval = 0;
#ifdef DSL_ASUSWRT
	char buf[32] = {0};
#endif
    
	memset(string, 0, SPRINT_MAX_LEN);
   
	if(strlen(passphrase))
		strcpy(UserInputKey,passphrase);
	else
		strcpy(UserInputKey, "");

	wPasswdLen = strlen(UserInputKey);

	if (subunit == 0)
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	else
		snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);

#ifdef ASUSWRT
	if(nvram_safe_get(strcat_r(prefix, "wep_x", tmp)) != NULL)
		tmpval = strtol(nvram_safe_get(strcat_r(prefix, "wep_x", tmp)), NULL, 10);
#else 	/* DSL_ASUSWRT */
	if(tcapi_get_string(WLAN_NODE, strcat_r(prefix, "wep_x", tmp), buf) != NULL)
		tmpval = strtol(tcapi_get_string(WLAN_NODE, strcat_r(prefix, "wep_x", tmp), buf), NULL, 10);
#endif
	
	if( tmpval==1 ) //64-bits
	{
		/* generate seed for random number generator using */
		/* key string... */
		for (i=0; i<wPasswdLen; i++) {
			pseed[i%4]^= UserInputKey[i];
		}

		/* init PRN generator... note that this is equivalent */
		/*  to the Microsoft srand() function. */
		randNumber =	(int)pseed[0] |
				((int)pseed[1])<<8 |
				((int)pseed[2])<<16 |
				((int)pseed[3])<<24;

		/* generate keys. */
		for (i=0; i<4; i++) {
			for (j=0; j<5; j++) {
				/* Note that these three lines are */
				/* equivalent to the Microsoft rand() */
				/* function. */
				randNumber *= 0x343fd;
				randNumber += 0x269ec3;
				wep_key[i][j] = (unsigned char)((randNumber>>16) & 0x7fff);
			}	
		}
		
		for (i=0; i<4; i++) 
		{
			memset(tmpstr, 0, SPRINT_MAX_LEN);
			//sprintf(tmpstr, "mssid_key%d_x%d", i+1, ssid_index);
			sprintf(tmpstr, "%skey%d", prefix, i+1);
			tmpstr[strlen(tmpstr)] = '\0';
			sprintf(string,"%02X%02X%02X%02X%02X",wep_key[i][0],wep_key[i][1],
				wep_key[i][2],wep_key[i][3],wep_key[i][4]);	
#ifdef ASUSWRT
			nvram_set(tmpstr,string);
#else 	/* DSL_ASUSWRT */
			tcapi_set(WLAN_NODE, tmpstr, string);
#endif 
		}	
	}//end of if( tmpval==1 )
	else if( tmpval==2 ) //128-bits
	{    	
		// assume 104-bit key and use MD5 for passphrase munging
		ASUS_MD5_CTX MD;
		char			*cp;
		char			password_buf[65];
		unsigned char 	key[16];
		int 			k;

		// Initialize MD5 structures     
		ASUS_MD5Init(&MD);

		// concatenate input passphrase repeatedly to fill password_buf
		cp = password_buf;
		for (i=0; i<64; i++)
		{
	    	if(wPasswdLen)
				*cp++ = UserInputKey[i % wPasswdLen];
			else
				*cp++ = UserInputKey[0];
		}

		// generate 128-bit signature using MD5
		ASUS_MD5Update(&MD, (unsigned char *)password_buf, 64);
		ASUS_MD5Final((unsigned char *)key, &MD);
		//  copy 13 bytes (104 bits) of MG5 generated signature to
		//  default key 0 (id = 1)
		k=0;
		for (i=0; i<3; i++)
			for (j=0; j<5; j++)
				wep_key[i][j] = key[k++];
			    
		for (i=0; i<4; i++) 
		{
			memset(tmpstr, 0, SPRINT_MAX_LEN);
			//sprintf(tmpstr, "mssid_key%d_x%d", i+1, ssid_index);
			sprintf(tmpstr, "%skey%d", prefix, i+1);
			tmpstr[strlen(tmpstr)] = '\0';
			sprintf(string,"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
							wep_key[0][0],wep_key[0][1],wep_key[0][2],wep_key[0][3],
							wep_key[0][4],wep_key[1][0],wep_key[1][1],wep_key[1][2],
							wep_key[1][3],wep_key[1][4],wep_key[2][0],wep_key[2][1],
							wep_key[2][2]);
#ifdef ASUSWRT
			nvram_set(tmpstr,string);
#else 	/* DSL_ASUSWRT */
			tcapi_set(WLAN_NODE, tmpstr, string);
#endif 
		}				    
		    
	}//end of else if( tmpval==2 )
}

unsigned long get_statistic_of_net_dev(char *net_dev, int field)
{
	FILE *fp;
	char buf[256];
	char *ifname;
	char *ptr;
	int res = 0;
	unsigned long count = 0;
	
	if((fp = fopen("/proc/net/dev", "r"))==NULL) return 0;

	fgets(buf, sizeof(buf), fp);
	fgets(buf, sizeof(buf), fp);

	while(fgets(buf, sizeof(buf), fp) != NULL) {
		if((ptr = strchr(buf, ':')) == NULL)
			continue;

		*ptr = 0;
		if((ifname = strrchr(buf, ' ')) == NULL)
			ifname = buf;
		else
			++ifname;

		if(strcmp(ifname, net_dev))
			continue;

		// <rx bytes, packets, errors, dropped, fifo errors, frame errors, compressed, multicast><tx bytes, packets, errors, dropped ...>
		switch(field) {
			case ETHERNET_BYTES_SENT:
				res = sscanf(ptr+1, "%*u%*u%*u%*u%*u%*u%*u%*u%lu%*u%*u%*u", &count);
				break;
			case ETHERNET_BYTES_RECEIVED:
				res = sscanf(ptr+1, "%lu%*u%*u%*u%*u%*u%*u%*u%*u%*u%*u%*u", &count);
				break;
			case ETHERNET_PACKETS_SENT:
				res = sscanf(ptr+1, "%*u%*u%*u%*u%*u%*u%*u%*u%*u%lu%*u%*u", &count);
				break;
			case ETHERNET_PACKETS_RECEIVED:
				res = sscanf(ptr+1, "%*u%lu%*u%*u%*u%*u%*u%*u%*u%*u%*u%*u", &count);
				break;
			case ETHERNET_ERRORS_SENT:
				res = sscanf(ptr+1, "%*u%*u%*u%*u%*u%*u%*u%*u%*u%*u%lu%*u", &count);
				break;
			case ETHERNET_ERRORS_RECEIVED:
				res = sscanf(ptr+1, "%*u%*u%lu%*u%*u%*u%*u%*u%*u%*u%*u%*u", &count);
				break;
			case ETHERNET_DISCARD_PACKETS_SENT:
				res = sscanf(ptr+1, "%*u%*u%*u%*u%*u%*u%*u%*u%*u%*u%*u%lu", &count);
				break;
			case ETHERNET_DISCARD_PACKETS_RECEIVED:
				res = sscanf(ptr+1, "%*u%*u%*u%lu%*u%*u%*u%*u%*u%*u%*u%*u", &count); 
				break;
		}
		
		if(res != 1) {
			fclose(fp);
			return count;
		}

		break;
	}
	fclose(fp);

	return count;
}

#ifdef ASUSWRT
#ifdef RTCONFIG_QTN
unsigned int get_statistic_of_qtn_dev(char *prefix, int field)
{
	char tmp[32] = {0};
	unsigned int count = 0;
	char qtn_ifname[16] = {0};

	if (!strcmp(prefix, "wl1_"))
		snprintf(qtn_ifname, sizeof(qtn_ifname), "%s", nvram_safe_get(strcat_r(prefix, "ifname", tmp)));
	else
		snprintf(qtn_ifname, sizeof(qtn_ifname), "wifi%d", atoi(prefix + 4));

	if (rpc_qtn_ready()) {
		switch(field) {
			case ETHERNET_BYTES_SENT:
				qcsapi_interface_get_counter(qtn_ifname, qcsapi_total_bytes_sent, &count);
				break;
			case ETHERNET_BYTES_RECEIVED:
				qcsapi_interface_get_counter(qtn_ifname, qcsapi_total_bytes_received, &count);
				break;
			case ETHERNET_PACKETS_SENT:
				qcsapi_interface_get_counter(qtn_ifname, qcsapi_total_packets_sent, &count);
				break;
			case ETHERNET_PACKETS_RECEIVED:
				qcsapi_interface_get_counter(qtn_ifname, qcsapi_total_packets_received, &count);
				break;
			case ETHERNET_ERRORS_SENT:
				qcsapi_interface_get_counter(qtn_ifname, qcsapi_error_packets_sent, &count);
				break;
			case ETHERNET_ERRORS_RECEIVED:
				qcsapi_interface_get_counter(qtn_ifname, qcsapi_error_packets_received, &count);
				break;
			case ETHERNET_DISCARD_PACKETS_SENT:
				qcsapi_interface_get_counter(qtn_ifname, qcsapi_discard_packets_sent, &count);
				break;
			case ETHERNET_DISCARD_PACKETS_RECEIVED:
				qcsapi_interface_get_counter(qtn_ifname, qcsapi_discard_packets_received, &count);
				break;
		}
	}

	return count;
}
#endif

unsigned int num_of_mssid_support(unsigned int unit)
{
	char tmp_vifnames[] = "wlx_vifnames";
	char word[64] = {0}, *next = NULL;
	int subunit;
#ifdef RTCONFIG_RALINK
	if (nvram_match("wl_mssid", "0"))
		return 0;
#endif
	snprintf(tmp_vifnames, sizeof(tmp_vifnames), "wl%d_vifnames", unit);

	subunit = 0;
	foreach (word, nvram_safe_get(tmp_vifnames), next) {
		subunit++;
	}

	return subunit;
}
#endif

int cal_cpuoccupy (CPU_OCCUPY *o, CPU_OCCUPY *n)  
{    
    unsigned long od, nd;     
    unsigned long id, sd; 
    int cpu_use = 0;    
    
    od = (unsigned long) (o->user + o->nice + o->system +o->idle);
    nd = (unsigned long) (n->user + n->nice + n->system + n->idle);
      
    id = (unsigned long) (n->user - o->user);
    sd = (unsigned long) (n->system - o->system);
    if ((nd-od) != 0) 
    	cpu_use = (int)((sd+id)*100)/(nd-od);
    else
    	cpu_use = 0; 
    //printf("cpu: %u\n",cpu_use); 
    return cpu_use; 
}

void get_cpuoccupy (CPU_OCCUPY *cpust)
{    
    FILE *fd;         
    char buff[256];  
    CPU_OCCUPY *cpu_occupy; 
    cpu_occupy = cpust; 
                                                                                                               
    fd = fopen ("/proc/stat", "r");  
    fgets (buff, sizeof (buff), fd); 
    
    sscanf (buff, "%s %u %u %u %u", cpu_occupy->name, &cpu_occupy->user, &cpu_occupy->nice, &cpu_occupy->system, &cpu_occupy->idle); 
    
    fclose(fd);      
}

int getInstNum( char *name, char *objname )
{
	int num=0;
	
	if( (objname!=NULL)  && (name!=NULL) )
	{
		char buf[256],*tok;
		sprintf( buf, ".%s.", objname );
		tok = strstr( name, buf );
		if(tok)
		{
			tok = tok + strlen(buf);
			sscanf( tok, "%d.%*s", &num );
		}
	}
	
	return num;
}

#ifdef DSL_ASUSWRT
/* Functions for DSL-ASUSWRT */
#define ALIGN_SIZE 1024
#define DEFAULT_HEADER "DSL-N66U"

//must be the same structure in /apps/private/cfg_manager/utility.c
typedef struct romfile_header_s
{
	char productName[16];
	char keyWord[16];  //encryptRomfile
	unsigned long fileLength;
	unsigned int rand;
}romfile_header_t;

//must be the same keyword in /apps/private/cfg_manager/utility.c
#define KEYWORD "EnCrYpTRomFIle"

unsigned char get_rand()
{
	unsigned char buf[1];
	FILE *fp;

	fp = fopen("/dev/urandom", "r");
	if(fp == NULL) {
		return 0;
	}
	fread(buf, 1, 1, fp);
	fclose(fp);

	return buf[0];
}

unsigned long readFileSize(char *filepath)
{
	struct stat sb;

	if(stat(filepath, &sb) == 0) {
		//on success, return file size
		return sb.st_size;
	}
	return 0;

}

/**
** src : source file path, ex: /tmp/var/romfile.cfg
** dst : destination file path, ex: /tmp/var/romfile_encrypt.cfg
** productName : the encrypted romfile header, string type.
** Encrypt the src file to the dst file, the src file will not be changed.
**/
int encryptRomfile(char *src, char *dst, char *productName)
{
	FILE *fp = NULL;
	//char cmd[128] = {0};
	unsigned long count, filelen, i;
	unsigned int rand = 0;
	//unsigned char temp;
	char *buffer = NULL;
	int srcFD = -1;
	int ret = 0;
	romfile_header_t rhdr;

	unlink(dst);
	if ((fp = fopen(dst, "w")) == NULL) return -1;

	if( ( srcFD = open(src, O_RDONLY) ) < 0 )
	{
		return -2;
	}
	count = readFileSize(src);
	buffer = (char *) calloc( count, sizeof(char));
	
	ret = read(srcFD, buffer, count);
	close(srcFD);
	if( ret < 0 )
	{
		free(buffer);
		fclose(fp);
		return -3;
	}

	memset(&rhdr, 0, sizeof(rhdr));
	strcpy(rhdr.keyWord, KEYWORD);
	rand = get_rand() % 30;
	rhdr.rand = rand;

	//currently ROMFILE_PADDING is not needed. We keep it just in case in the future.
#if defined(ROMFILE_PADDING)
	filelen = count + (ALIGN_SIZE - (count%ALIGN_SIZE));
#else
	filelen = count;
#endif
	rhdr.fileLength = filelen;
	
	if((!productName)||( strlen(productName) <= 0 ))
	{
		strncpy(rhdr.productName, DEFAULT_HEADER, sizeof(rhdr.productName) );
	}
	else
	{
		strncpy(rhdr.productName, productName, sizeof(rhdr.productName) );
		rhdr.productName[sizeof(rhdr.productName)-1] = '\0';
	}

	//write header
	fwrite(&rhdr, 1, sizeof(rhdr), fp);

	//encrypt data
	for (i = 0; i < count; i++)
	{
		if (buffer[i] == 0x0)
			buffer[i] = 0xfd + get_rand() % 3;
		else
			buffer[i] = 0xff - buffer[i] + rand;
	}
	
	//write data
	fwrite(buffer, 1, count, fp);

#if defined(ROMFILE_PADDING)
	//paddings
	for (i = count; i < filelen; i++)
	{
		temp = 0xfd + get_rand() % 3;
		fwrite(&temp, 1, 1, fp);
	}
#endif
	
	fclose(fp);
	free(buffer);
	return 0;
}

int handleRomfile()
{
	char noEncrypt[32] = {0}, productName[64] = {0}, dstCfgPath[64];
	int ret = 0;
	char *srcCfgPath = "/tmp/var/romfile.cfg";

	ret = tcapi_get("SysInfo_Entry", "ProductName", productName);
	if(ret < 0)
		return 0;

	snprintf(dstCfgPath, sizeof(dstCfgPath), "/tmp/Settings_%s.cfg", productName);

	ret = tcapi_get("WebCurSet_Entry", "NE", noEncrypt); //NE means NoEncrypt
	if((ret >= 0) && (!strcmp(noEncrypt, "1"))) {
		eval("cp", srcCfgPath, dstCfgPath);
		return 0;
	}
	else
	{
		if(strlen(productName) <= 0)
			encryptRomfile(srcCfgPath, dstCfgPath, "DSL-N66U");
		else
			encryptRomfile(srcCfgPath, dstCfgPath, productName);
	}

	return 0;
}

int decryptRomfile(char *path, unsigned int *length, unsigned int offset)
{
	FILE *tmp_fp = NULL;
	struct stat st;
	char *buffer = NULL;
	char *fileName = NULL;
	romfile_header_t hdr;
	int srcFD = -1;
	int ret = -1;
	char cmd[256] = {0};
	unsigned long i;
	unsigned int rand;
	int needDecrypt = 0, noEncrypt = 0;
	int err = 0;
	char productName[16] = {0};
	char value[16] = {0};

	memset(&hdr, 0, sizeof(hdr));
	if( !path )
	{
		err = -1;
		goto errorHandle;
	}
	if(stat(path, &st) != 0)
	{
		err = -2;
		goto errorHandle;
	}
	if( ( srcFD = open(path, O_RDONLY) ) < 0 )
	{
		err = -3;
		goto errorHandle;
	}
	
	fileName = (char *)calloc(strlen(path)+5, sizeof(char));
	if( !fileName )
	{
		err = -4;
		goto errorHandle;
	}
	sprintf(fileName, "%s.tmp", path );
	tmp_fp = fopen(fileName, "w");
	if( !tmp_fp )
	{
		err = -5;
		goto errorHandle;
	}

	buffer = (char *)calloc(st.st_size, sizeof(char));
	if(!buffer)
	{
		err = -6;
		goto errorHandle;
	}
	ret = read(srcFD, buffer, st.st_size);
	if( ret < 0 )
	{
		err = -7;
		goto errorHandle;
	}
	close(srcFD);
	srcFD = -1; //after close, clean the file descriptor

	memcpy(&hdr, buffer+offset, sizeof(hdr));

	memset(value, 0, sizeof(value));
	//if(EZgetAttrValue("WebCurSet", "Entry", "NE", value) != TCAPI_PROCESS_OK)
	ret = tcapi_get("WebCurSet_Entry", "NE", value); //NE means NoEncrypt
	if((ret >= 0) && (!strcmp(value, "1")))
		noEncrypt = 1;
	else
		noEncrypt = 0;
	
	if(noEncrypt)
	{
		err = 0; //bypass checking mechanism
		goto errorHandle;
	}

	needDecrypt = 0;
	if(strcmp(hdr.keyWord, KEYWORD)==0)
	{
		memset(productName, 0, sizeof(productName));
		//if(EZgetAttrValue("SysInfo", "Entry", "ProductName", productName) != TCAPI_PROCESS_OK)
		ret = tcapi_get("SysInfo_Entry", "ProductName", productName);
		if(ret < 0)
		{
			err = -8;
			goto errorHandle;
		}
		else
		{
			if(strcmp(hdr.productName, productName)!=0)
			{
				err = -9; //the uploaded romfile does not belong to this model.
				goto errorHandle;
			}
		}
		
		//there is keyword in header.
		needDecrypt = 1;
	}

	//decrypt
	if( needDecrypt )
	{
		rand = hdr.rand;
		for( i = offset + sizeof(hdr); i < offset + (*length); i++ )
		{
			if ((unsigned char) buffer[i] > ( 0xfd - 0x1))
				buffer[i] = 0x0;
			else
				buffer[i] = 0xff + rand - buffer[i];
		}
		fwrite( buffer, 1, offset, tmp_fp );
		fwrite( buffer+offset+sizeof(hdr), 1, (*length)-sizeof(hdr), tmp_fp );
		fwrite( buffer+offset+(*length), 1, st.st_size-offset-(*length), tmp_fp );
		*length -= sizeof(hdr);

		unlink(path);
		sprintf(cmd, "mv %s %s", fileName, path );
		system(cmd);
	}

errorHandle:
	fclose(tmp_fp);
	free(fileName);
	free(buffer);
	close(srcFD);
	return err;
}
#endif	/* DSL_ASUSWRT */

static int in_cksum(unsigned short *buf, int sz)
{
	int nleft = sz;
	int sum = 0;
	unsigned short *w = buf;
	unsigned short ans = 0;

	while (nleft > 1) {
		sum += *w++;
		nleft -= 2;
	}

	if (nleft == 1) {
		*(unsigned char *) (&ans) = *(unsigned char *) w;
		sum += ans;
	}

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	ans = ~sum;
	return (ans);
}

//#define ERR_RESOURCE -1 
//#define ERR_UNREACHABLE -2

int icmp_test(char *intf, char *host, unsigned int count, unsigned int timeout, unsigned int datasize, unsigned char tos,
	unsigned int *cntOK, unsigned int *cntFail, unsigned int *timeAvg, unsigned int *timeMin, unsigned int *timeMax, unsigned int needWaitRsp) 
{
	struct protoent *proto;
	struct sockaddr_in sockaddr;
	struct hostent *h;
	struct icmp *icmppkt;
	struct timeval tv;

	
	unsigned char *buffer;
	fd_set rset;	
	int sock, bufsize, ret, pingid, cnt;
	//unsigned int attempt;
	unsigned short uSequence = 0, uExpected;
	unsigned int uOK = 0, uFail = 0;
	unsigned int tAvg = 0, tMin = 0, tMax = 0;
	int int_op;


	// create socket
	proto = getprotobyname("icmp");
	
	if ((sock = socket(AF_INET, SOCK_RAW, (proto ? proto->p_proto : IPPROTO_ICMP))) < 0) {        /* 1 == ICMP */
		//return ERR_RESOURCE;
		goto error_resource;
	}

	int_op = tos;
	if ((ret = setsockopt(sock, IPPROTO_IP, IP_TOS, (char *)&int_op, sizeof (int_op))) < 0) {
		//printf("set QoS %d returns %d\n", int_op, ret);
	}

	if (intf) {
		ret = setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, intf, strlen(intf)+1);
		if (ret != 0) 
			printf("failed to bind to %s. ret=%d\n", intf, ret);
	}

	memset(&sockaddr, 0, sizeof(struct sockaddr_in));
	
	sockaddr.sin_family = AF_INET;

	// resolve host name	
	if ((h = gethostbyname(host)) == NULL) {		
		//printf("ping: Get HostbyName Error\n");		
		//return ERR_UNREACHABLE;
		goto error_host;
	}
	
	if (h->h_addrtype != AF_INET) {
		//printf("unknown address type; only AF_INET is currently supported.\n");
		//return ERR_UNREACHABLE;
		goto error_resource;
		
	}
	
	memcpy(&sockaddr.sin_addr, h->h_addr, sizeof(sockaddr.sin_addr));

	printf("PING %s (%s): %d data bytes\n",
           h->h_name,
           inet_ntoa(*(struct in_addr *) &sockaddr.sin_addr.s_addr), datasize);

	FD_ZERO(&rset);
       FD_SET(sock, &rset);

	// build the packet
	bufsize = sizeof(icmppkt) + datasize;
       buffer = malloc ( bufsize );
       if (0==buffer) {
       	printf("no buffer available\n");
       	goto error_resource;
       	//return ERR_RESOURCE;
       }

	pingid = getpid() & 0xFFFF;

	cnt = 0;
	while (count > 0) {
		struct timeval time1, time2;
		
		count --;		

		uSequence++;
		uExpected = uSequence;
		icmppkt = (struct icmp *) buffer;
		icmppkt->icmp_type = ICMP_ECHO;
		icmppkt->icmp_code = 0;
		icmppkt->icmp_cksum = 0;
		icmppkt->icmp_seq = uSequence;
		icmppkt->icmp_id = pingid;
		icmppkt->icmp_cksum = in_cksum((unsigned short *) icmppkt, bufsize);
		
		ret = sendto(sock, buffer, bufsize, MSG_DONTWAIT,  (struct sockaddr *) &sockaddr, sizeof(struct sockaddr_in));

		if ((ret < 0) && (ret != EAGAIN)) {
			uFail++;
			continue;			
		}

		if(needWaitRsp ==1){
		gettimeofday(&time1, 0);

		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
		
		ret = select(sock+1, &rset, NULL, NULL, &tv);
		if (ret==0) { // timeout
			uFail++;

			// debug info.
			//gettimeofday(&time2, 0);
			//printf("timeout: %u.%u\n", time2.tv_sec, time2.tv_usec);
			
		} else if (ret > 0) {
			int c, hlen;
			struct sockaddr_in from;
			struct iphdr *ippkt;
			struct icmp *pkt;
			unsigned int delta;
			socklen_t fromlen = (socklen_t) sizeof(from);
			c = recvfrom(sock, buffer, bufsize, 0,  (struct sockaddr *) &from, &fromlen);
			if (c < 0) { // recv error
				uFail++;
				printf("recv returns %d\n", c);
				continue;
			}

			gettimeofday(&time2, 0);
			
			// size is wrong
			if (c < bufsize) {
				uFail++;
				printf("size is wrong %d,%d\n", c, bufsize);
				continue;
			}

			ippkt = (struct iphdr *)buffer;
			hlen = ippkt->ihl << 2;
			pkt = (struct icmp *) (buffer + hlen);
#ifdef ASUSWRT	
			if (pkt->icmp_id != pingid)
#else 	/* DSL_ASUSWRT */
			if (ntohs(pkt->icmp_id) != pingid)
#endif
			{
				uFail++;
				//printf("ping id mismatch %d,%d\n", pingid, ntohs(pkt->icmp_id));
				continue;
			}

			if (pkt->icmp_type != ICMP_ECHOREPLY) {
				uFail++;
				//printf("not icmp\n");
				continue;
			}

#ifdef ASUSWRT
			if (pkt->icmp_seq != uExpected) 
#else 	/* DSL_ASUSWRT */			
			if (ntohs(pkt->icmp_seq) != uExpected) 
#endif
			{
				uFail++;
				//printf("seq mismatch %d, %d\n", ntohs(pkt->icmp_seq), uExpected);
				continue;
			}
			cnt++;

			delta = (time2.tv_sec * 1000 + time2.tv_usec / 1000) - (time1.tv_sec * 1000 + time1.tv_usec / 1000);
			
			if (cnt == 1) {
				tAvg = tMin = tMax = delta;
			} else {
				tAvg = tAvg + (int)(delta - tAvg) / cnt;
				if (tMin > delta) tMin = delta;
				if (tMax < delta)tMax = delta;			
			}
					
			uOK++;

			
		}
			}	
	
	}

	if (cntOK) *cntOK = uOK;
	if (cntFail) *cntFail = uFail;
	if (uOK > 0) {
		if (timeAvg) *timeAvg = tAvg;
		if (timeMin) *timeMin= tMin;
		if (timeMax) *timeMax = tMax;
	} else {
		if (timeAvg) *timeAvg = 0;
		if (timeMin) *timeMin= 0;
		if (timeMax) *timeMax = 0;
	}
	
	free(buffer);
	close(sock);
	return DIAG_COMPLETE;
error_resource:
	close(sock);
	return DIAG_ERROR_INTERNAL;
error_host:
	close(sock);
	return DIAG_ERROR_CANNOT_RESOLVE_HOST_NAME;
	
	
}

unsigned char get_rand()
{
    unsigned char buf[1];
    FILE *fp;

    fp = fopen("/dev/urandom", "r");
    if (fp == NULL) {
#ifdef ASUS_DEBUG
        fprintf(stderr, "Could not open /dev/urandom.\n");
#endif
        return 0;
    }
    fread(buf, 1, 1, fp);
    fclose(fp);

    return buf[0];
}
