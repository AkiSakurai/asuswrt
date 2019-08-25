#ifdef BCMCCX

/*
 *   bcmccx.c - BCM CCX utility functions
 *
 * Broadcom Proprietary and Confidential. Copyright (C) 2017,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 * $Id: bcmccx.c 451682 2014-01-27 20:30:17Z $
 */

#include <typedefs.h>
#include <bcmendian.h>
#include <bcmutils.h>

#ifdef BCMDRIVER
#include <osl.h>
#else
#include <string.h>
#endif /* BCMDRIVER */

#include <bcmcrypto/md4.h>
#include <bcmcrypto/md5.h>
#include <bcmcrypto/des.h>
#include <bcmcrypto/ccx.h>
#include <bcmcrypto/bcmccx.h>


/* Extrapolated from an example in Cisco Client Extensions, version 1.11,
 * on the assumption that it's published there so it could be used.
 */
void
bcm_ccx_hashpwd(uint8 *pwd, size_t pw_len, uint8 hash[CCX_PW_HASH_LEN],
                uint8 hashhash[CCX_PW_HASH_LEN])
{
	MD4_CTX md4context;
	uint8 unicode_pw[256 * 2];
	int i;

	memset((char *)hash, 0, CCX_PW_HASH_LEN);
	memset((char *)hashhash, 0, CCX_PW_HASH_LEN);
	memset((char *)unicode_pw, 0, sizeof(unicode_pw));

	for (i = 0; i < (int) pw_len; i++)
		unicode_pw[i * 2] = *pwd++;

	MD4Init(&md4context);
	MD4Update(&md4context, unicode_pw, pw_len * 2 * 8);
	MD4Final(hash, &md4context);

	MD4Init(&md4context);
	MD4Update(&md4context, hash, CCX_PW_HASH_LEN * 8);
	MD4Final(hashhash, &md4context);
}

void
bcm_ccx_session_key(uint8 *inbuf, size_t in_len,
                    uint8 outbuf[CCX_SESSION_KEY_LEN])
{
	MD5_CTX md5context;

	MD5Init(&md5context);
	MD5Update(&md5context, inbuf, in_len);
	MD5Final(outbuf, &md5context);
}

/* extracts 7 bits from an input byte string for DES key */
static uint8
get7bits(const uint8 *src, int bitpos)
{
	uint16 v;

	v = ((uint16)(src[bitpos/8]) << 8) + ((uint16)(src[bitpos/8+1]) << 0);
	return (uint8)(v >> (8 - (bitpos & 7)) & 0xfe);
}

void
bcm_ccx_leap_response(uint8 pwhash[CCX_PW_HASH_LEN],
                      uint8 challenge[LEAP_CHALLENGE_LEN],
                      uint8 response[LEAP_RESPONSE_LEN])
{
	uint8 padded21[21], padded24[24], tmp[LEAP_CHALLENGE_LEN];
	DES_KS ks;
	uint i;

	memset(padded21, 0, sizeof(padded21));
	memcpy(padded21, pwhash, CCX_PW_HASH_LEN);

	/* now extract seven bits at a time to get 24 octets */
	for (i = 0; i < sizeof(padded24); i++)
		padded24[i] = get7bits(padded21, 7*i);

	for (i = 0; i < LEAP_RESPONSE_LEN; i += 8) {
		/* Encrypt is done in-place, so put input text where
		 * incremental output belongs and save a copy.
		 */
		memcpy(tmp, challenge, LEAP_CHALLENGE_LEN);
		deskey(ks, &padded24[i], 0);
		des(ks, tmp);
		memcpy(&response[i], tmp, sizeof(tmp));
	}
}

bool
bcm_ckip_rxseq_check(uint32 seq_odd, uint32 *rxseq_base, uint32 *rxseq_bitmap)
{
	uint32 seq, seq_offset, seq_bit, diff;

	if ((seq_odd % 2) == 0) {
		/* wrong sequence direction for AP->STA */
		return FALSE;
	}

	seq = seq_odd >> 1;
	seq_offset = (seq - *rxseq_base) & BCM_CKIP_RXSEQ_MASK;

	if (seq_offset < BCM_CKIP_RXSEQ_WIN) {
		seq_bit = (1 << seq_offset);
		if (*rxseq_bitmap & seq_bit) {
			/* sequence replay, discard */
			return FALSE;
		} else {
			*rxseq_bitmap |= seq_bit;
			return TRUE;
		}
	} else if (seq_offset <= (BCM_CKIP_RXSEQ_MASK / 2)) {
		/* if the sequence offset is less than half the sequence number range,
		 * consider it a sequence number ahead of us
		 */

		/* seq number advanced past the window so move the window up */
		diff = seq_offset - BCM_CKIP_RXSEQ_WIN_TARGET;
		if (diff < NBITS(*rxseq_bitmap))
			*rxseq_bitmap >>= diff;
		else {
			/* sequence advance */
			*rxseq_bitmap = 0;
		}
		*rxseq_base += diff;
		*rxseq_bitmap |= 1 << BCM_CKIP_RXSEQ_WIN_TARGET;
		return TRUE;
	} else {
		/* if the sequence offset is over half the sequence number range,
		 * consider it an old sequence number
		 */
		return FALSE;
	}
}

#endif /* BCMCCX */
