/*
 * Encode functions which provides encoding of information elements
 * as defined in 802.11.
 *
 * Broadcom Proprietary and Confidential. Copyright (C) 2016,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 * $Id:$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "proto/802.11.h"
#include "trace.h"
#include "bcm_hspot.h"
#include "bcm_encode_ie.h"

/* encode hotspot 2.0 indication */
int bcm_encode_ie_hotspot_indication(bcm_encode_t *pkt, uint8 hotspotConfig)
{
	int initLen = bcm_encode_length(pkt);

	bcm_encode_byte(pkt, DOT11_MNG_VS_ID);
	bcm_encode_byte(pkt, 5);
	bcm_encode_bytes(pkt, WFA_OUI_LEN, (uint8 *)WFA_OUI);
	bcm_encode_byte(pkt, HSPOT_IE_OUI_TYPE);
	bcm_encode_byte(pkt, hotspotConfig);

	return bcm_encode_length(pkt) - initLen;
}

/* encode hotspot 2.0 indication release2 */
int bcm_encode_ie_hotspot_indication2(bcm_encode_t *pkt,
	int isDgafDisabled, uint8 releaseNumber,
	int isPpsMoIdPresent, uint16 ppsMoId,
	int isAnqpDomainIdPresent, uint16 anqpDomainId)
{
	int initLen = bcm_encode_length(pkt);
	uint8 len;
	uint8 config = 0;

	bcm_encode_byte(pkt, DOT11_MNG_VS_ID);
	len = 5;
	if (isPpsMoIdPresent)
		len += 2;
	if (isAnqpDomainIdPresent)
		len += 2;
	bcm_encode_byte(pkt, len);
	bcm_encode_bytes(pkt, WFA_OUI_LEN, (uint8 *)WFA_OUI);
	bcm_encode_byte(pkt, HSPOT_IE_OUI_TYPE);
	if (isDgafDisabled)
		config |= HSPOT_DGAF_DISABLED_MASK;
	config |= (releaseNumber << HSPOT_RELEASE_SHIFT) & HSPOT_RELEASE_MASK;
	if (isPpsMoIdPresent)
		config |= HSPOT_PPS_MO_ID_MASK;
	if (isAnqpDomainIdPresent)
		config |= HSPOT_ANQP_DOMAIN_ID_MASK;
	bcm_encode_byte(pkt, config);
	if (isPpsMoIdPresent) {
		bcm_encode_le16(pkt, ppsMoId);
	}
	if (isAnqpDomainIdPresent) {
		bcm_encode_le16(pkt, anqpDomainId);
	}

	return bcm_encode_length(pkt) - initLen;
}

/* encode interworking */
int bcm_encode_ie_interworking(bcm_encode_t *pkt, uint8 accessNetworkType,
	int isInternet, int isAsra, int isEsr, int isUesa,
	int isVenue, uint8 venueGroup, uint8 venueType, struct ether_addr *hessid)
{
	int initLen = bcm_encode_length(pkt);
	int len = 1;
	uint8 options = 0;

	bcm_encode_byte(pkt, DOT11_MNG_INTERWORKING_ID);

	if (isVenue)
		len += 2;
	if (hessid != 0)
		len += sizeof(*hessid);
	bcm_encode_byte(pkt, len);

	options = accessNetworkType & IW_ANT_MASK;
	if (isInternet)
		options |= IW_INTERNET_MASK;
	if (isAsra)
		options |= IW_ASRA_MASK;
	if (isEsr)
		options |= IW_ESR_MASK;
	if (isUesa)
		options |= IW_UESA_MASK;
	bcm_encode_byte(pkt, options);

	if (isVenue) {
		bcm_encode_byte(pkt, venueGroup);
		bcm_encode_byte(pkt, venueType);
	}

	if (hessid != 0)
		bcm_encode_bytes(pkt, sizeof(*hessid), hessid->octet);

	return bcm_encode_length(pkt) - initLen;
}

/* encode advertisement protocol tuple */
int bcm_encode_ie_advertisement_protocol_tuple(bcm_encode_t *pkt,
	int isPamebi, uint8 qResponseLimit, uint8 protocolId)
{
	int initLen = bcm_encode_length(pkt);
	uint8 info;

	info = qResponseLimit & ADVP_QRL_MASK;
	if (isPamebi)
		info |= ADVP_PAME_BI_MASK;
	bcm_encode_byte(pkt, info);
	bcm_encode_byte(pkt, protocolId);

	return bcm_encode_length(pkt) - initLen;
}

/* encode advertisement protocol */
int bcm_encode_ie_advertisement_protocol_from_tuple(bcm_encode_t *pkt, uint8 len, uint8 *data)
{
	int initLen = bcm_encode_length(pkt);

	bcm_encode_byte(pkt, DOT11_MNG_ADVERTISEMENT_ID);
	bcm_encode_byte(pkt, len);
	if (len > 0) {
		bcm_encode_bytes(pkt, len, data);
	}

	return bcm_encode_length(pkt) - initLen;
}

/* encode roaming consortium */
int bcm_encode_ie_roaming_consortium(bcm_encode_t *pkt, uint8 numAnqpOi,
	uint8 oi1Len, uint8 *oi1, uint8 oi2Len, uint8 *oi2,
	uint8 oi3Len, uint8 *oi3)
{
	int initLen = bcm_encode_length(pkt);

	bcm_encode_byte(pkt, DOT11_MNG_ROAM_CONSORT_ID);
	bcm_encode_byte(pkt, 2 + oi1Len + oi2Len + oi3Len);
	bcm_encode_byte(pkt, numAnqpOi);
	bcm_encode_byte(pkt, oi2Len << 4 | (oi1Len & 0xf));
	if (oi1Len > 0)
		bcm_encode_bytes(pkt, oi1Len, oi1);
	if (oi2Len > 0)
		bcm_encode_bytes(pkt, oi2Len, oi2);
	if (oi3Len > 0)
		bcm_encode_bytes(pkt, oi3Len, oi3);

	return bcm_encode_length(pkt) - initLen;
}

/* encode extended capabilities */
int bcm_encode_ie_extended_capabilities(bcm_encode_t *pkt, uint32 cap)
{
	int initLen = bcm_encode_length(pkt);

	bcm_encode_byte(pkt, DOT11_MNG_EXT_CAP_ID);
	bcm_encode_byte(pkt, 4);
	bcm_encode_le32(pkt, cap);

	return bcm_encode_length(pkt) - initLen;
}

/* encode advertisement protocol */
int bcm_encode_ie_advertisement_protocol(bcm_encode_t *pkt,
	uint8 pamebi, uint8 qRspLimit, uint8 id)
{
	int initLen = bcm_encode_length(pkt);

	bcm_encode_byte(pkt, DOT11_MNG_ADVERTISEMENT_ID);
	bcm_encode_byte(pkt, 2);
	bcm_encode_byte(pkt, (pamebi << 7) | (qRspLimit & 0x7f));
	bcm_encode_byte(pkt, id);

	return bcm_encode_length(pkt) - initLen;
}

/* encode qbss load */
int bcm_encode_ie_bss_load(bcm_encode_t *pkt, uint16 stationCount,
	uint8 channelUtilization, uint16 availableAdmissionCapacity)
{
	int initLen = bcm_encode_length(pkt);

	bcm_encode_byte(pkt, DOT11_MNG_QBSS_LOAD_ID);
	bcm_encode_byte(pkt, 5);
	bcm_encode_le16(pkt, stationCount);
	bcm_encode_byte(pkt, channelUtilization);
	bcm_encode_le16(pkt, availableAdmissionCapacity);

	return bcm_encode_length(pkt) - initLen;
}
