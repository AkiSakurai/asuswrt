/*
 * Decode functions which provides decoding of P2P attributes
 * as defined in P2P specification.
 *
 * Broadcom Proprietary and Confidential. Copyright (C) 2017,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 * $Id:$
 */

#include "proto/p2p.h"
#include "proto/wps.h"
#include "bcmutils.h"
#ifdef BCMDBG
#include "devctrl_if/wlioctl_defs.h"
#endif
#include "wl_dbg.h"
#include "bcm_decode_p2p.h"

static void printWfdDecode(bcm_decode_p2p_t *wfd)
{
	WL_P2PO(("decoded P2P IEs:\n"));

#ifndef BCMDRIVER	/* not used in dongle */
	if (wfd->statusBuffer) {
		WL_PRPKT("   P2P_SEID_STATUS",
			wfd->statusBuffer, wfd->statusLength);
	}
	if (wfd->minorReasonCodeBuffer) {
		WL_PRPKT("   P2P_SEID_MINOR_RC",
			wfd->minorReasonCodeBuffer, wfd->minorReasonCodeLength);
	}
#endif	/* BCMDRIVER */
	if (wfd->capabilityBuffer) {
		WL_PRPKT("   P2P_SEID_P2P_INFO",
			wfd->capabilityBuffer, wfd->capabilityLength);
	}
#ifndef BCMDRIVER	/* not used in dongle */
	if (wfd->deviceIdBuffer) {
		WL_PRPKT("   P2P_SEID_DEV_ID",
			wfd->deviceIdBuffer, wfd->deviceIdLength);
	}
	if (wfd->groupOwnerIntentBuffer) {
		WL_PRPKT("   P2P_SEID_INTENT",
			wfd->groupOwnerIntentBuffer, wfd->groupOwnerIntentLength);
	}
	if (wfd->configurationTimeoutBuffer) {
		WL_PRPKT("   P2P_SEID_CFG_TIMEOUT",
			wfd->configurationTimeoutBuffer, wfd->configurationTimeoutLength);
	}
	if (wfd->listenChannelBuffer) {
		WL_PRPKT("   P2P_SEID_CHANNEL",
			wfd->listenChannelBuffer, wfd->listenChannelLength);
	}
	if (wfd->groupBssidBuffer) {
		WL_PRPKT("   P2P_SEID_GRP_BSSID",
			wfd->groupBssidBuffer, wfd->groupBssidLength);
	}
	if (wfd->extendedListenTimingBuffer) {
		WL_PRPKT("   P2P_SEID_XT_TIMING",
			wfd->extendedListenTimingBuffer, wfd->extendedListenTimingLength);
	}
	if (wfd->intendedInterfaceAddressBuffer) {
		WL_PRPKT("   P2P_SEID_INTINTADDR",
			wfd->intendedInterfaceAddressBuffer, wfd->intendedInterfaceAddressLength);
	}
	if (wfd->manageabilityBuffer) {
		WL_PRPKT("   P2P_SEID_P2P_MGBTY",
			wfd->manageabilityBuffer, wfd->manageabilityLength);
	}
	if (wfd->channelListBuffer) {
		WL_PRPKT("   P2P_SEID_CHAN_LIST",
			wfd->channelListBuffer, wfd->channelListLength);
	}
	if (wfd->noticeOfAbsenseBuffer) {
		WL_PRPKT("   P2P_SEID_ABSENCE",
			wfd->noticeOfAbsenseBuffer, wfd->noticeOfAbsenceLength);
	}
#endif	/* BCMDRIVER */
	if (wfd->deviceInfoBuffer) {
		WL_PRPKT("   P2P_SEID_DEV_INFO",
			wfd->deviceInfoBuffer, wfd->deviceInfoLength);
	}
#ifndef BCMDRIVER	/* not used in dongle */
	if (wfd->groupInfoBuffer) {
		WL_PRPKT("   P2P_SEID_GROUP_INFO",
			wfd->groupInfoBuffer, wfd->groupInfoLength);
	}
	if (wfd->groupIdBuffer) {
		WL_PRPKT("   P2P_SEID_GROUP_ID",
			wfd->groupIdBuffer, wfd->groupIdLength);
	}
	if (wfd->interfaceBuffer) {
		WL_PRPKT("   P2P_SEID_P2P_IF",
			wfd->interfaceBuffer, wfd->interfaceLength);
	}
	if (wfd->operatingChannelBuffer) {
		WL_PRPKT("   P2P_SEID_OP_CHANNEL",
			wfd->operatingChannelBuffer, wfd->operatingChannelLength);
	}
	if (wfd->invitationFlagsBuffer) {
		WL_PRPKT("   P2P_SEID_INVITE_FLAGS",
			wfd->invitationFlagsBuffer, wfd->invitationFlagsLength);
	}
#endif	/* BCMDRIVER */
}

/* decode P2P */
int bcm_decode_p2p(bcm_decode_t *pkt, bcm_decode_p2p_t *wfd)
{
	uint8 oui[WFA_OUI_LEN];
	uint8 type;
	int nextIeOffset = 0;
	int ieCount = 0;

	WL_PRPKT("packet for P2P decoding",
		bcm_decode_current_ptr(pkt), bcm_decode_remaining(pkt));

	memset(wfd, 0, sizeof(*wfd));

	/* check OUI */
	if (!bcm_decode_bytes(pkt, WFA_OUI_LEN, oui) ||
		memcmp(oui, WFA_OUI, WFA_OUI_LEN) != 0) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	/* check type */
	if (!bcm_decode_byte(pkt, &type) || type != WFA_OUI_TYPE_P2P) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	nextIeOffset = bcm_decode_offset(pkt);

	while (nextIeOffset < bcm_decode_buf_length(pkt)) {
		uint8 id;
		uint16 length;
		int dataLength;
		uint8 *dataPtr;

		bcm_decode_offset_set(pkt, nextIeOffset);
		WL_TRACE(("decoding offset 0x%x\n", bcm_decode_offset(pkt)));

		/* minimum ID and length */
		if (bcm_decode_remaining(pkt) < 3) {
			WL_P2PO(("ID and length too short\n"));
			break;
		}

		(void)bcm_decode_byte(pkt, &id);
		(void)bcm_decode_le16(pkt, &length);

		/* check length */
		if (length > bcm_decode_remaining(pkt)) {
			WL_P2PO(("length exceeds packet %d > %d\n",
				length, bcm_decode_remaining(pkt)));
			break;
		}
		nextIeOffset = bcm_decode_offset(pkt) + length;

		/* data */
		dataLength = length;
		dataPtr = bcm_decode_current_ptr(pkt);

		switch (id)
		{
#ifndef BCMDRIVER	/* not used in dongle */
		case P2P_SEID_STATUS:
			wfd->statusLength = dataLength;
			wfd->statusBuffer = dataPtr;
			break;
		case P2P_SEID_MINOR_RC:
			wfd->minorReasonCodeLength = dataLength;
			wfd->minorReasonCodeBuffer = dataPtr;
			break;
#endif	/* BCMDRIVER */
		case P2P_SEID_P2P_INFO:
			wfd->capabilityLength = dataLength;
			wfd->capabilityBuffer = dataPtr;
			break;
#ifndef BCMDRIVER	/* not used in dongle */
		case P2P_SEID_DEV_ID:
			wfd->deviceIdLength = dataLength;
			wfd->deviceIdBuffer = dataPtr;
			break;
		case P2P_SEID_INTENT:
			wfd->groupOwnerIntentLength = dataLength;
			wfd->groupOwnerIntentBuffer = dataPtr;
			break;
		case P2P_SEID_CFG_TIMEOUT:
			wfd->configurationTimeoutLength = dataLength;
			wfd->configurationTimeoutBuffer = dataPtr;
			break;
		case P2P_SEID_CHANNEL:
			wfd->listenChannelLength = dataLength;
			wfd->listenChannelBuffer = dataPtr;
			break;
		case P2P_SEID_GRP_BSSID:
			wfd->groupBssidLength = dataLength;
			wfd->groupBssidBuffer = dataPtr;
			break;
		case P2P_SEID_XT_TIMING:
			wfd->extendedListenTimingLength = dataLength;
			wfd->extendedListenTimingBuffer = dataPtr;
			break;
		case P2P_SEID_INTINTADDR:
			wfd->intendedInterfaceAddressLength = dataLength;
			wfd->intendedInterfaceAddressBuffer = dataPtr;
			break;
		case P2P_SEID_P2P_MGBTY:
			wfd->manageabilityLength = dataLength;
			wfd->manageabilityBuffer = dataPtr;
			break;
		case P2P_SEID_CHAN_LIST:
			wfd->channelListLength = dataLength;
			wfd->channelListBuffer = dataPtr;
			break;
		case P2P_SEID_ABSENCE:
			wfd->noticeOfAbsenceLength = dataLength;
			wfd->noticeOfAbsenseBuffer = dataPtr;
			break;
#endif 	/* BCMDRIVER */
		case P2P_SEID_DEV_INFO:
			wfd->deviceInfoLength = dataLength;
			wfd->deviceInfoBuffer = dataPtr;
			break;
#ifndef BCMDRIVER	/* not used in dongle */
		case P2P_SEID_GROUP_INFO:
			wfd->groupInfoLength = dataLength;
			wfd->groupInfoBuffer = dataPtr;
			break;
		case P2P_SEID_GROUP_ID:
			wfd->groupIdLength = dataLength;
			wfd->groupIdBuffer = dataPtr;
			break;
		case P2P_SEID_P2P_IF:
			wfd->interfaceLength = dataLength;
			wfd->interfaceBuffer = dataPtr;
			break;
		case P2P_SEID_OP_CHANNEL:
			wfd->operatingChannelLength = dataLength;
			wfd->operatingChannelBuffer = dataPtr;
			break;
		case P2P_SEID_INVITE_FLAGS:
			wfd->invitationFlagsLength = dataLength;
			wfd->invitationFlagsBuffer = dataPtr;
			break;
		case P2P_SEID_SERVICE_HASH:
			wfd->serviceHashLength = dataLength;
			wfd->serviceHashBuffer = dataPtr;
			break;
		case P2P_SEID_SESSION:
			wfd->sessionLength = dataLength;
			wfd->sessionBuffer = dataPtr;
			break;
		case P2P_SEID_CONNECT_CAP:
			wfd->connectCapLength = dataLength;
			wfd->connectCapBuffer = dataPtr;
			break;
		case P2P_SEID_ADVERTISE_ID:
			wfd->advertiseIdLength = dataLength;
			wfd->advertiseIdBuffer = dataPtr;
			break;
#endif	/* BCMDRIVER */
		case P2P_SEID_ADVERTISE_SERVICE:
			wfd->advertiseServiceLength = dataLength;
			wfd->advertiseServiceBuffer = dataPtr;
			break;
#ifndef BCMDRIVER	/* not used in dongle */
		case P2P_SEID_SESSION_ID:
			wfd->sessionIdLength = dataLength;
			wfd->sessionIdBuffer = dataPtr;
			break;
		case P2P_SEID_FEATURE_CAP:
			wfd->featureCapLength = dataLength;
			wfd->featureCapBuffer = dataPtr;
			break;
		case P2P_SEID_PERSISTENT_GROUP:
			wfd->persistentGroupLength = dataLength;
			wfd->persistentGroupBuffer = dataPtr;
			break;
#endif	/* BCMDRIVER */

		default:
			WL_P2PO(("invalid ID %d\n", id));
			continue;
			break;
		}

		/* count IEs decoded */
		ieCount++;
	}

	if (ieCount > 0)
		printWfdDecode(wfd);

	return ieCount;
}

/* decode device info */
int bcm_decode_p2p_device_info(bcm_decode_t *pkt, bcm_decode_p2p_device_info_t *device)
{
	int i;
	uint16 type;
	uint16 length;

	WL_PRPKT("packet for P2P device info decoding",
		bcm_decode_current_ptr(pkt), bcm_decode_remaining(pkt));

	memset(device, 0, sizeof(*device));

	/* allow zero length */
	if (bcm_decode_remaining(pkt) == 0)
		return TRUE;

	if (!bcm_decode_bytes(pkt, 6, (uint8 *)&device->deviceAddress))
	{
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!bcm_decode_be16(pkt, &device->configMethods)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!bcm_decode_bytes(pkt, 8, (uint8 *)&device->primaryType))
	{
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!bcm_decode_byte(pkt, &device->numSecondaryType))
	{
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (device->numSecondaryType > BCM_DECODE_P2P_MAX_SECONDARY_DEVICE_TYPE)
	{
		WL_ERROR(("num secondary device type %d > %d\n",
			device->numSecondaryType, BCM_DECODE_P2P_MAX_SECONDARY_DEVICE_TYPE));
		return FALSE;
	}

	for (i = 0; i < device->numSecondaryType; i++)
	{
		if (!bcm_decode_bytes(pkt, 8, (uint8 *)&device->secondaryType[i]))
		{
			WL_ERROR(("decode error\n"));
			return FALSE;
		}
	}

	if (!bcm_decode_be16(pkt, &type) || type != WPS_ID_DEVICE_NAME) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (!bcm_decode_be16(pkt, &length)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (length > BCM_DECODE_P2P_MAX_DEVICE_NAME)
	{
		WL_ERROR(("device name length %d > %d\n",
			length, BCM_DECODE_P2P_MAX_DEVICE_NAME));
		return FALSE;
	}
	if (bcm_decode_bytes(pkt, length, device->deviceName) != length)
	{
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	device->deviceName[length] = 0;

	return TRUE;
}

/* print decoded device info */
void bcm_decode_p2p_device_info_print(bcm_decode_p2p_device_info_t *device)
{
	int i;

	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded P2P device info:\n"));
	WL_PRUSR("   device address",
		(uint8 *)&device->deviceAddress, sizeof(device->deviceAddress));
	WL_PRINT(("   config methods = 0x%04x\n", device->configMethods));
	WL_PRUSR("   primary device type",
		(uint8 *)&device->primaryType, sizeof(device->primaryType));
	WL_PRINT(("   num secondary device type = %d\n", device->numSecondaryType));
	for (i = 0; i < device->numSecondaryType; i++) {
		WL_PRUSR("   secondary device type",
			(uint8 *)&device->secondaryType[i], sizeof(device->secondaryType[i]));
	}
	WL_PRUSR("   device name",
		(uint8 *)&device->deviceName, strlen((char *)device->deviceName));
}

/* decode capability */
int bcm_decode_p2p_capability(bcm_decode_t *pkt, bcm_decode_p2p_capability_t *capability)
{
	WL_PRPKT("packet for P2P capability decoding",
		bcm_decode_current_ptr(pkt), bcm_decode_remaining(pkt));

	memset(capability, 0, sizeof(*capability));

	if (bcm_decode_remaining(pkt) != 2)
		return FALSE;

	if (!bcm_decode_byte(pkt, &capability->device))
	{
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!bcm_decode_byte(pkt, &capability->group))
	{
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	return TRUE;
}

/* print decoded capability */
void bcm_decode_p2p_capability_print(bcm_decode_p2p_capability_t *capability)
{
	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded P2P capability:\n"));
	WL_PRINT(("   device = 0x%02x\n", capability->device));
	WL_PRINT(("   group  = 0x%02x\n", capability->group));
}
