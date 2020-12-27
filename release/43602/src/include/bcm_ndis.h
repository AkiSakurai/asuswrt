/*
 * Definitions redundant to Windows DDK header file
 * definitions needed to build RNDIS drivers for non-
 * Windows platforms.
 *
 * Portions
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: bcm_ndis.h 404499 2013-05-28 01:06:37Z $
 */

/*FILE-CSTYLED*/

#ifndef _bcm_ndis_h_
#define _bcm_ndis_h_

#ifndef NDIS

#include <typedefs.h>

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

/* Basic types */
typedef char CHAR, *PCHAR;
typedef short SHORT, *PSHORT;
typedef int INT, *PINT;
typedef long LONG, *PLONG;
typedef long long LONGLONG, *PLONGLONG;
typedef unsigned char UCHAR, *PUCHAR;
typedef unsigned short USHORT, *PUSHORT;
typedef unsigned short WCHAR, *PWCHAR;
typedef unsigned int UINT, *PUINT;
typedef unsigned long ULONG, *PULONG;
typedef unsigned long long ULONGLONG, *PULONGLONG;

/* Basic sized types */
typedef int8 INT8, *PINT8;
typedef int16 INT16, *PINT16;
typedef int32 INT32, *PINT32;
typedef int64 INT64, *PINT64;
typedef uint8 UINT8, *PUINT8;
typedef uint16 UINT16, *PUINT16;
typedef uint32 UINT32, *PUINT32;
typedef uint64 UINT64, *PUINT64;

/* Signed 32-bit wide types */
typedef int32 LONG32, *PLONG32;

/* Unsigned 32-bit wide types */
typedef uint32 ULONG32, *PULONG32;
typedef uint32 DWORD32, *PDWORD32;

/* Void */
#define VOID void
typedef void *PVOID;

/* Boolean */
typedef bool BOOLEAN;

/* Handle */
typedef void *HANDLE;
typedef void *NDIS_HANDLE;

/* Same size as a pointer */
typedef unsigned int UINT_PTR, *PUINT_PTR;
typedef unsigned long ULONG_PTR, *PULONG_PTR;

/* Large integer */
typedef struct _LARGE_INTEGER {
	long long QuadPart;
} LARGE_INTEGER;

#endif /* !NDIS */

//
// This is the type of an NDIS OID value.
//

typedef ULONG NDIS_OID, *PNDIS_OID;

#ifndef _NDIS_
typedef int NDIS_STATUS, *PNDIS_STATUS;
#endif

#define STATUS_SUCCESS                          ((NDIS_STATUS)0x00000000L) // ntsubauth
#define STATUS_PENDING                   ((NDIS_STATUS)0x00000103L)    // winnt
#define STATUS_UNSUCCESSFUL              ((NDIS_STATUS)0xC0000001L)
#define STATUS_BUFFER_OVERFLOW           ((NDIS_STATUS)0x80000005L)
#define STATUS_INSUFFICIENT_RESOURCES    ((NDIS_STATUS)0xC000009AL)     // ntsubauth
#define STATUS_NOT_SUPPORTED             ((NDIS_STATUS)0xC00000BBL)
#define STATUS_INVALID_DEVICE_REQUEST    ((NDIS_STATUS)0xC0000010L)
#define STATUS_NETWORK_UNREACHABLE       ((NDIS_STATUS)0xC000023CL)

//
// NDIS_STATUS values
//

#define NDIS_STATUS_SUCCESS                     ((NDIS_STATUS)STATUS_SUCCESS)
#define NDIS_STATUS_PENDING                     ((NDIS_STATUS) STATUS_PENDING)
#define NDIS_STATUS_NOT_RECOGNIZED              ((NDIS_STATUS)0x00010001L)
#define NDIS_STATUS_NOT_COPIED                  ((NDIS_STATUS)0x00010002L)
#define NDIS_STATUS_NOT_ACCEPTED                ((NDIS_STATUS)0x00010003L)
#define NDIS_STATUS_CALL_ACTIVE                 ((NDIS_STATUS)0x00010007L)

#define NDIS_STATUS_ONLINE                      ((NDIS_STATUS)0x40010003L)
#define NDIS_STATUS_RESET_START                 ((NDIS_STATUS)0x40010004L)
#define NDIS_STATUS_RESET_END                   ((NDIS_STATUS)0x40010005L)
#define NDIS_STATUS_RING_STATUS                 ((NDIS_STATUS)0x40010006L)
#define NDIS_STATUS_CLOSED                      ((NDIS_STATUS)0x40010007L)
#define NDIS_STATUS_WAN_LINE_UP                 ((NDIS_STATUS)0x40010008L)
#define NDIS_STATUS_WAN_LINE_DOWN               ((NDIS_STATUS)0x40010009L)
#define NDIS_STATUS_WAN_FRAGMENT                ((NDIS_STATUS)0x4001000AL)
#define NDIS_STATUS_MEDIA_CONNECT               ((NDIS_STATUS)0x4001000BL)
#define NDIS_STATUS_MEDIA_DISCONNECT            ((NDIS_STATUS)0x4001000CL)
#define NDIS_STATUS_HARDWARE_LINE_UP            ((NDIS_STATUS)0x4001000DL)
#define NDIS_STATUS_HARDWARE_LINE_DOWN          ((NDIS_STATUS)0x4001000EL)
#define NDIS_STATUS_INTERFACE_UP                ((NDIS_STATUS)0x4001000FL)
#define NDIS_STATUS_INTERFACE_DOWN              ((NDIS_STATUS)0x40010010L)
#define NDIS_STATUS_MEDIA_BUSY                  ((NDIS_STATUS)0x40010011L)
#define NDIS_STATUS_MEDIA_SPECIFIC_INDICATION   ((NDIS_STATUS)0x40010012L)
#define NDIS_STATUS_WW_INDICATION               NDIS_STATUS_MEDIA_SPECIFIC_INDICATION
#define NDIS_STATUS_LINK_SPEED_CHANGE           ((NDIS_STATUS)0x40010013L)
#define NDIS_STATUS_WAN_GET_STATS               ((NDIS_STATUS)0x40010014L)
#define NDIS_STATUS_WAN_CO_FRAGMENT             ((NDIS_STATUS)0x40010015L)
#define NDIS_STATUS_WAN_CO_LINKPARAMS           ((NDIS_STATUS)0x40010016L)

#define NDIS_STATUS_NOT_RESETTABLE              ((NDIS_STATUS)0x80010001L)
#define NDIS_STATUS_SOFT_ERRORS                 ((NDIS_STATUS)0x80010003L)
#define NDIS_STATUS_HARD_ERRORS                 ((NDIS_STATUS)0x80010004L)
#define NDIS_STATUS_BUFFER_OVERFLOW             ((NDIS_STATUS)STATUS_BUFFER_OVERFLOW)

#define NDIS_STATUS_INVALID_STATE		((NDIS_STATUS)0xC0000184L)

#define NDIS_STATUS_FAILURE                     ((NDIS_STATUS) STATUS_UNSUCCESSFUL)
#define NDIS_STATUS_RESOURCES                   ((NDIS_STATUS)STATUS_INSUFFICIENT_RESOURCES)
#define NDIS_STATUS_CLOSING                     ((NDIS_STATUS)0xC0010002L)
#define NDIS_STATUS_BAD_VERSION                 ((NDIS_STATUS)0xC0010004L)
#define NDIS_STATUS_BAD_CHARACTERISTICS         ((NDIS_STATUS)0xC0010005L)
#define NDIS_STATUS_ADAPTER_NOT_FOUND           ((NDIS_STATUS)0xC0010006L)
#define NDIS_STATUS_OPEN_FAILED                 ((NDIS_STATUS)0xC0010007L)
#define NDIS_STATUS_DEVICE_FAILED               ((NDIS_STATUS)0xC0010008L)
#define NDIS_STATUS_MULTICAST_FULL              ((NDIS_STATUS)0xC0010009L)
#define NDIS_STATUS_MULTICAST_EXISTS            ((NDIS_STATUS)0xC001000AL)
#define NDIS_STATUS_MULTICAST_NOT_FOUND         ((NDIS_STATUS)0xC001000BL)
#define NDIS_STATUS_REQUEST_ABORTED             ((NDIS_STATUS)0xC001000CL)
#define NDIS_STATUS_RESET_IN_PROGRESS           ((NDIS_STATUS)0xC001000DL)
#define NDIS_STATUS_CLOSING_INDICATING          ((NDIS_STATUS)0xC001000EL)
#define NDIS_STATUS_NOT_SUPPORTED               ((NDIS_STATUS)STATUS_NOT_SUPPORTED)
#define NDIS_STATUS_INVALID_PACKET              ((NDIS_STATUS)0xC001000FL)
#define NDIS_STATUS_OPEN_LIST_FULL              ((NDIS_STATUS)0xC0010010L)
#define NDIS_STATUS_ADAPTER_NOT_READY           ((NDIS_STATUS)0xC0010011L)
#define NDIS_STATUS_ADAPTER_NOT_OPEN            ((NDIS_STATUS)0xC0010012L)
#define NDIS_STATUS_NOT_INDICATING              ((NDIS_STATUS)0xC0010013L)
#define NDIS_STATUS_INVALID_LENGTH              ((NDIS_STATUS)0xC0010014L)
#define NDIS_STATUS_INVALID_DATA                ((NDIS_STATUS)0xC0010015L)
#define NDIS_STATUS_BUFFER_TOO_SHORT            ((NDIS_STATUS)0xC0010016L)
#define NDIS_STATUS_INVALID_OID                 ((NDIS_STATUS)0xC0010017L)
#define NDIS_STATUS_ADAPTER_REMOVED             ((NDIS_STATUS)0xC0010018L)
#define NDIS_STATUS_UNSUPPORTED_MEDIA           ((NDIS_STATUS)0xC0010019L)
#define NDIS_STATUS_GROUP_ADDRESS_IN_USE        ((NDIS_STATUS)0xC001001AL)
#define NDIS_STATUS_FILE_NOT_FOUND              ((NDIS_STATUS)0xC001001BL)
#define NDIS_STATUS_ERROR_READING_FILE          ((NDIS_STATUS)0xC001001CL)
#define NDIS_STATUS_ALREADY_MAPPED              ((NDIS_STATUS)0xC001001DL)
#define NDIS_STATUS_RESOURCE_CONFLICT           ((NDIS_STATUS)0xC001001EL)
#define NDIS_STATUS_NO_CABLE                    ((NDIS_STATUS)0xC001001FL)

#define NDIS_STATUS_INVALID_SAP                 ((NDIS_STATUS)0xC0010020L)
#define NDIS_STATUS_SAP_IN_USE                  ((NDIS_STATUS)0xC0010021L)
#define NDIS_STATUS_INVALID_ADDRESS             ((NDIS_STATUS)0xC0010022L)
#define NDIS_STATUS_VC_NOT_ACTIVATED            ((NDIS_STATUS)0xC0010023L)
#define NDIS_STATUS_DEST_OUT_OF_ORDER           ((NDIS_STATUS)0xC0010024L)  // cause 27
#define NDIS_STATUS_VC_NOT_AVAILABLE            ((NDIS_STATUS)0xC0010025L)  // cause 35,45
#define NDIS_STATUS_CELLRATE_NOT_AVAILABLE      ((NDIS_STATUS)0xC0010026L)  // cause 37
#define NDIS_STATUS_INCOMPATABLE_QOS            ((NDIS_STATUS)0xC0010027L)  // cause 49
#define NDIS_STATUS_AAL_PARAMS_UNSUPPORTED      ((NDIS_STATUS)0xC0010028L)  // cause 93
#define NDIS_STATUS_NO_ROUTE_TO_DESTINATION     ((NDIS_STATUS)0xC0010029L)  // cause 3

#define NDIS_STATUS_TOKEN_RING_OPEN_ERROR       ((NDIS_STATUS)0xC0011000L)
#define NDIS_STATUS_INVALID_DEVICE_REQUEST      ((NDIS_STATUS)STATUS_INVALID_DEVICE_REQUEST)
#define NDIS_STATUS_NETWORK_UNREACHABLE         ((NDIS_STATUS)STATUS_NETWORK_UNREACHABLE)

//
// Ndis Packet Filter Bits (OID_GEN_CURRENT_PACKET_FILTER).
//
#define NDIS_PACKET_TYPE_DIRECTED               0x00000001
#define NDIS_PACKET_TYPE_MULTICAST              0x00000002
#define NDIS_PACKET_TYPE_ALL_MULTICAST          0x00000004
#define NDIS_PACKET_TYPE_BROADCAST              0x00000008
#define NDIS_PACKET_TYPE_SOURCE_ROUTING         0x00000010
#define NDIS_PACKET_TYPE_PROMISCUOUS            0x00000020
#define NDIS_PACKET_TYPE_SMT                    0x00000040
#define NDIS_PACKET_TYPE_ALL_LOCAL              0x00000080
#define NDIS_PACKET_TYPE_GROUP                  0x00001000
#define NDIS_PACKET_TYPE_ALL_FUNCTIONAL         0x00002000
#define NDIS_PACKET_TYPE_FUNCTIONAL             0x00004000
#define NDIS_PACKET_TYPE_MAC_FRAME              0x00008000

//
//  Required OIDs
//
#define OID_GEN_SUPPORTED_LIST                  0x00010101
#define OID_GEN_HARDWARE_STATUS                 0x00010102
#define OID_GEN_MEDIA_SUPPORTED                 0x00010103
#define OID_GEN_MEDIA_IN_USE                    0x00010104
#define OID_GEN_MAXIMUM_LOOKAHEAD               0x00010105
#define OID_GEN_MAXIMUM_FRAME_SIZE              0x00010106
#define OID_GEN_LINK_SPEED                      0x00010107
#define OID_GEN_TRANSMIT_BUFFER_SPACE           0x00010108
#define OID_GEN_RECEIVE_BUFFER_SPACE            0x00010109
#define OID_GEN_TRANSMIT_BLOCK_SIZE             0x0001010A
#define OID_GEN_RECEIVE_BLOCK_SIZE              0x0001010B
#define OID_GEN_VENDOR_ID                       0x0001010C
#define OID_GEN_VENDOR_DESCRIPTION              0x0001010D
#define OID_GEN_CURRENT_PACKET_FILTER           0x0001010E
#define OID_GEN_CURRENT_LOOKAHEAD               0x0001010F
#define OID_GEN_DRIVER_VERSION                  0x00010110
#define OID_GEN_MAXIMUM_TOTAL_SIZE              0x00010111
#define OID_GEN_PROTOCOL_OPTIONS                0x00010112
#define OID_GEN_MAC_OPTIONS                     0x00010113
#define OID_GEN_MEDIA_CONNECT_STATUS            0x00010114
#define OID_GEN_MAXIMUM_SEND_PACKETS            0x00010115
#define OID_GEN_VENDOR_DRIVER_VERSION           0x00010116
#define OID_GEN_SUPPORTED_GUIDS                 0x00010117
#define OID_GEN_NETWORK_LAYER_ADDRESSES         0x00010118  // Set only
#define OID_GEN_TRANSPORT_HEADER_OFFSET         0x00010119  // Set only
#define OID_GEN_MACHINE_NAME                    0x0001021A
#define OID_GEN_RNDIS_CONFIG_PARAMETER          0x0001021B  // Set only
#define OID_GEN_VLAN_ID                         0x0001021C

//
//  Optional OIDs
//
#define OID_GEN_MEDIA_CAPABILITIES              0x00010201
#define OID_GEN_PHYSICAL_MEDIUM                 0x00010202

#define OID_GEN_RNDIS_CONFIG_PARAMETER          0x0001021B  // Set only

//
//  Required statistics
//
#define OID_GEN_XMIT_OK                         0x00020101
#define OID_GEN_RCV_OK                          0x00020102
#define OID_GEN_XMIT_ERROR                      0x00020103
#define OID_GEN_RCV_ERROR                       0x00020104
#define OID_GEN_RCV_NO_BUFFER                   0x00020105

//
//  Optional statistics
//
#define OID_GEN_DIRECTED_BYTES_XMIT             0x00020201
#define OID_GEN_DIRECTED_FRAMES_XMIT            0x00020202
#define OID_GEN_MULTICAST_BYTES_XMIT            0x00020203
#define OID_GEN_MULTICAST_FRAMES_XMIT           0x00020204
#define OID_GEN_BROADCAST_BYTES_XMIT            0x00020205
#define OID_GEN_BROADCAST_FRAMES_XMIT           0x00020206
#define OID_GEN_DIRECTED_BYTES_RCV              0x00020207
#define OID_GEN_DIRECTED_FRAMES_RCV             0x00020208
#define OID_GEN_MULTICAST_BYTES_RCV             0x00020209
#define OID_GEN_MULTICAST_FRAMES_RCV            0x0002020A
#define OID_GEN_BROADCAST_BYTES_RCV             0x0002020B
#define OID_GEN_BROADCAST_FRAMES_RCV            0x0002020C

#define OID_GEN_RCV_CRC_ERROR                   0x0002020D
#define OID_GEN_TRANSMIT_QUEUE_LENGTH           0x0002020E

#define OID_GEN_GET_TIME_CAPS                   0x0002020F
#define OID_GEN_GET_NETCARD_TIME                0x00020210
#define OID_GEN_NETCARD_LOAD                    0x00020211
#define OID_GEN_DEVICE_PROFILE                  0x00020212

//
// The following is exported by NDIS itself and is only queryable. It returns
// the time in milliseconds a driver took to initialize.
//
#define OID_GEN_INIT_TIME_MS                    0x00020213
#define OID_GEN_RESET_COUNTS                    0x00020214
#define OID_GEN_MEDIA_SENSE_COUNTS              0x00020215
#define OID_GEN_FRIENDLY_NAME                   0x00020216
#define OID_GEN_MINIPORT_INFO                   0x00020217
#define OID_GEN_RESET_VERIFY_PARAMETERS         0x00020218

//
// 802.3 Objects (Ethernet)
//
#define OID_802_3_PERMANENT_ADDRESS             0x01010101
#define OID_802_3_CURRENT_ADDRESS               0x01010102
#define OID_802_3_MULTICAST_LIST                0x01010103
#define OID_802_3_MAXIMUM_LIST_SIZE             0x01010104
#define OID_802_3_MAC_OPTIONS                   0x01010105

#define NDIS_802_3_MAC_OPTION_PRIORITY          0x00000001

#define OID_802_3_RCV_ERROR_ALIGNMENT           0x01020101
#define OID_802_3_XMIT_ONE_COLLISION            0x01020102
#define OID_802_3_XMIT_MORE_COLLISIONS          0x01020103

#define OID_802_3_XMIT_DEFERRED                 0x01020201
#define OID_802_3_XMIT_MAX_COLLISIONS           0x01020202
#define OID_802_3_RCV_OVERRUN                   0x01020203
#define OID_802_3_XMIT_UNDERRUN                 0x01020204
#define OID_802_3_XMIT_HEARTBEAT_FAILURE        0x01020205
#define OID_802_3_XMIT_TIMES_CRS_LOST           0x01020206
#define OID_802_3_XMIT_LATE_COLLISIONS          0x01020207
//
//  PnP and PM OIDs
//
#define OID_PNP_CAPABILITIES                    0xFD010100
#define OID_PNP_SET_POWER                       0xFD010101
#define OID_PNP_QUERY_POWER                     0xFD010102
#define OID_PNP_ADD_WAKE_UP_PATTERN             0xFD010103
#define OID_PNP_REMOVE_WAKE_UP_PATTERN          0xFD010104
#define OID_PNP_WAKE_UP_PATTERN_LIST            0xFD010105
#define OID_PNP_ENABLE_WAKE_UP                  0xFD010106

//
//  PnP/PM Statistics (Optional).
//
#define OID_PNP_WAKE_UP_OK                      0xFD020200
#define OID_PNP_WAKE_UP_ERROR                   0xFD020201

//
//  The following bits are defined for OID_PNP_ENABLE_WAKE_UP
//
#define NDIS_PNP_WAKE_UP_MAGIC_PACKET           0x00000001
#define NDIS_PNP_WAKE_UP_PATTERN_MATCH          0x00000002
#define NDIS_PNP_WAKE_UP_LINK_CHANGE            0x00000004

//
//  The following structure defines the device power states.
//
typedef enum _NDIS_DEVICE_POWER_STATE
{
    NdisDeviceStateUnspecified = 0,
    NdisDeviceStateD0,
    NdisDeviceStateD1,
    NdisDeviceStateD2,
    NdisDeviceStateD3,
    NdisDeviceStateMaximum
} NDIS_DEVICE_POWER_STATE, *PNDIS_DEVICE_POWER_STATE;

//
//  The following structure defines the wake-up capabilities of the device.
//
typedef struct _NDIS_PM_WAKE_UP_CAPABILITIES
{
    NDIS_DEVICE_POWER_STATE MinMagicPacketWakeUp;
    NDIS_DEVICE_POWER_STATE MinPatternWakeUp;
    NDIS_DEVICE_POWER_STATE MinLinkChangeWakeUp;
} NDIS_PM_WAKE_UP_CAPABILITIES, *PNDIS_PM_WAKE_UP_CAPABILITIES;

//
// the following flags define the -enabled- wake-up capabilities of the device
// passed in the Flags field of NDIS_PNP_CAPABILITIES structure
//
#define NDIS_DEVICE_WAKE_UP_ENABLE                          0x00000001
#define NDIS_DEVICE_WAKE_ON_PATTERN_MATCH_ENABLE            0x00000002
#define NDIS_DEVICE_WAKE_ON_MAGIC_PACKET_ENABLE             0x00000004

//
//  This structure defines general PnP capabilities of the miniport driver.
//
typedef struct _NDIS_PNP_CAPABILITIES
{
    ULONG                           Flags;
    NDIS_PM_WAKE_UP_CAPABILITIES    WakeUpCapabilities;
} NDIS_PNP_CAPABILITIES, *PNDIS_PNP_CAPABILITIES;


//
// Ndis MAC option bits (OID_GEN_MAC_OPTIONS).
//
#define NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA             0x00000001
#define NDIS_MAC_OPTION_RECEIVE_SERIALIZED              0x00000002
#define NDIS_MAC_OPTION_TRANSFERS_NOT_PEND              0x00000004
#define NDIS_MAC_OPTION_NO_LOOPBACK                     0x00000008
#define NDIS_MAC_OPTION_FULL_DUPLEX                     0x00000010
#define NDIS_MAC_OPTION_EOTX_INDICATION                 0x00000020
#define NDIS_MAC_OPTION_8021P_PRIORITY                  0x00000040
#define NDIS_MAC_OPTION_SUPPORTS_MAC_ADDRESS_OVERWRITE  0x00000080
#define NDIS_MAC_OPTION_RECEIVE_AT_DPC                  0x00000100
#define NDIS_MAC_OPTION_8021Q_VLAN                      0x00000200
#define NDIS_MAC_OPTION_RESERVED                        0x80000000

//
//  TCP/IP OIDs
//
#define OID_TCP_TASK_OFFLOAD                    0xFC010201
#define OID_TCP_TASK_IPSEC_ADD_SA               0xFC010202
#define OID_TCP_TASK_IPSEC_DELETE_SA            0xFC010203
#define OID_TCP_SAN_SUPPORT                     0xFC010204

typedef struct _NDIS_802_11_FIXED_IEs
{
	UCHAR Timestamp[8];
	USHORT BeaconInterval;
	USHORT Capabilities;
} NDIS_802_11_FIXED_IEs, *PNDIS_802_11_FIXED_IEs;

//
// IEEE 802.11 OIDs
//
#ifndef EXT_STA
#define OID_802_11_BSSID                        0x0D010101
#define OID_802_11_SSID                         0x0D010102
#define OID_802_11_NETWORK_TYPES_SUPPORTED      0x0D010203
#define OID_802_11_NETWORK_TYPE_IN_USE          0x0D010204
#define OID_802_11_TX_POWER_LEVEL               0x0D010205
#define OID_802_11_RSSI                         0x0D010206
#define OID_802_11_RSSI_TRIGGER                 0x0D010207
#define OID_802_11_INFRASTRUCTURE_MODE          0x0D010108
#define OID_802_11_FRAGMENTATION_THRESHOLD      0x0D010209
#define OID_802_11_RTS_THRESHOLD                0x0D01020A
#define OID_802_11_NUMBER_OF_ANTENNAS           0x0D01020B
#define OID_802_11_RX_ANTENNA_SELECTED          0x0D01020C
#define OID_802_11_TX_ANTENNA_SELECTED          0x0D01020D
#define OID_802_11_SUPPORTED_RATES              0x0D01020E
#define OID_802_11_DESIRED_RATES                0x0D010210
#define OID_802_11_CONFIGURATION                0x0D010211
#define OID_802_11_STATISTICS                   0x0D020212
#define OID_802_11_ADD_WEP                      0x0D010113
#define OID_802_11_REMOVE_WEP                   0x0D010114
#define OID_802_11_DISASSOCIATE                 0x0D010115
#define OID_802_11_POWER_MODE                   0x0D010216
#define OID_802_11_BSSID_LIST                   0x0D010217
#define OID_802_11_AUTHENTICATION_MODE          0x0D010118
#define OID_802_11_PRIVACY_FILTER               0x0D010119
#define OID_802_11_BSSID_LIST_SCAN              0x0D01011A
#define OID_802_11_WEP_STATUS                   0x0D01011B
// Renamed to support more than just WEP encryption
#define OID_802_11_ENCRYPTION_STATUS            OID_802_11_WEP_STATUS
#define OID_802_11_RELOAD_DEFAULTS              0x0D01011C
// Added to allow key mapping and default keys
#define OID_802_11_ADD_KEY                      0x0D01011D
#define OID_802_11_REMOVE_KEY                   0x0D01011E
#define OID_802_11_ASSOCIATION_INFORMATION      0x0D01011F
#define OID_802_11_TEST                         0x0D010120
#define OID_802_11_CAPABILITY					0x0D010122
#define OID_802_11_PMKID						0x0D010123

//
// IEEE 802.11 Structures and definitions
//
// new types for Media Specific Indications

typedef enum _NDIS_802_11_STATUS_TYPE
{
    Ndis802_11StatusType_Authentication,
    Ndis802_11StatusType_MediaStreamMode,
    Ndis802_11StatusType_PMKID_CandidateList,
    Ndis802_11StatusTypeMax    // not a real type, defined as an upper bound
} NDIS_802_11_STATUS_TYPE, *PNDIS_802_11_STATUS_TYPE;

typedef UCHAR   NDIS_802_11_MAC_ADDRESS[6];

typedef struct _NDIS_802_11_STATUS_INDICATION
{
    NDIS_802_11_STATUS_TYPE StatusType;
} NDIS_802_11_STATUS_INDICATION, *PNDIS_802_11_STATUS_INDICATION;

// mask for authentication/integrity fields
#define NDIS_802_11_AUTH_REQUEST_AUTH_FIELDS		0x0f

#define NDIS_802_11_AUTH_REQUEST_REAUTH				0x01
#define NDIS_802_11_AUTH_REQUEST_KEYUPDATE			0x02
#define NDIS_802_11_AUTH_REQUEST_PAIRWISE_ERROR		0x06
#define NDIS_802_11_AUTH_REQUEST_GROUP_ERROR		0x0E

typedef struct _NDIS_802_11_AUTHENTICATION_REQUEST
{
    ULONG Length;            // Length of structure
    NDIS_802_11_MAC_ADDRESS Bssid;
	char Reserved[2];		// BCM - added for explicit alignment
    ULONG Flags;
} NDIS_802_11_AUTHENTICATION_REQUEST, *PNDIS_802_11_AUTHENTICATION_REQUEST;

//Added new types for PMKID Candidate lists.
typedef struct _PMKID_CANDIDATE {
    NDIS_802_11_MAC_ADDRESS BSSID;
	char Reserved[2];		// BCM - added for explicit alignment
    ULONG Flags;
} PMKID_CANDIDATE, *PPMKID_CANDIDATE;

typedef struct _NDIS_802_11_PMKID_CANDIDATE_LIST
{
    ULONG Version;       // Version of the structure
    ULONG NumCandidates; // No. of pmkid candidates
    PMKID_CANDIDATE CandidateList[1];
} NDIS_802_11_PMKID_CANDIDATE_LIST, *PNDIS_802_11_PMKID_CANDIDATE_LIST;

//Flags for PMKID Candidate list structure
#define NDIS_802_11_PMKID_CANDIDATE_PREAUTH_ENABLED	0x01

//
// IEEE 802.11 Structures and definitions
//

typedef enum _NDIS_802_11_NETWORK_TYPE
{
    Ndis802_11FH,
    Ndis802_11DS,
    Ndis802_11OFDM5,
    Ndis802_11OFDM24,
    Ndis802_11Automode,
    Ndis802_11NetworkTypeMax    // not a real type, defined as an upper bound
} NDIS_802_11_NETWORK_TYPE, *PNDIS_802_11_NETWORK_TYPE;

typedef struct _NDIS_802_11_NETWORK_TYPE_LIST
{
    ULONG                       NumberOfItems;  // in list below, at least 1
    NDIS_802_11_NETWORK_TYPE    NetworkType [1];
} NDIS_802_11_NETWORK_TYPE_LIST, *PNDIS_802_11_NETWORK_TYPE_LIST;

typedef enum _NDIS_802_11_POWER_MODE
{
    Ndis802_11PowerModeCAM,
    Ndis802_11PowerModeMAX_PSP,
    Ndis802_11PowerModeFast_PSP,
    Ndis802_11PowerModeMax      // not a real mode, defined as an upper bound
} NDIS_802_11_POWER_MODE, *PNDIS_802_11_POWER_MODE;

typedef ULONG   NDIS_802_11_TX_POWER_LEVEL; // in milliwatts

//
// Received Signal Strength Indication
//
typedef LONG   NDIS_802_11_RSSI;           // in dBm

typedef struct _NDIS_802_11_CONFIGURATION_FH
{
    ULONG           Length;             // Length of structure
    ULONG           HopPattern;         // As defined by 802.11, MSB set
    ULONG           HopSet;             // to one if non-802.11
    ULONG           DwellTime;          // units are Kusec
} NDIS_802_11_CONFIGURATION_FH, *PNDIS_802_11_CONFIGURATION_FH;

typedef struct _NDIS_802_11_CONFIGURATION
{
    ULONG           Length;             // Length of structure
    ULONG           BeaconPeriod;       // units are Kusec
    ULONG           ATIMWindow;         // units are Kusec
    ULONG           DSConfig;           // Frequency, units are kHz
    NDIS_802_11_CONFIGURATION_FH    FHConfig;
} NDIS_802_11_CONFIGURATION, *PNDIS_802_11_CONFIGURATION;

typedef struct _NDIS_802_11_STATISTICS
{
    ULONG           Length;             // Length of structure
	char Reserved[4];					// BCM - added for explicit alignment
    LARGE_INTEGER   TransmittedFragmentCount;
    LARGE_INTEGER   MulticastTransmittedFrameCount;
    LARGE_INTEGER   FailedCount;
    LARGE_INTEGER   RetryCount;
    LARGE_INTEGER   MultipleRetryCount;
    LARGE_INTEGER   RTSSuccessCount;
    LARGE_INTEGER   RTSFailureCount;
    LARGE_INTEGER   ACKFailureCount;
    LARGE_INTEGER   FrameDuplicateCount;
    LARGE_INTEGER   ReceivedFragmentCount;
    LARGE_INTEGER   MulticastReceivedFrameCount;
    LARGE_INTEGER   FCSErrorCount;
    LARGE_INTEGER   TKIPLocalMICFailures;
    LARGE_INTEGER   TKIPRemoteMICErrors;
    LARGE_INTEGER   TKIPCounterMeasuresInvoked;
    LARGE_INTEGER   TKIPReplays;
    LARGE_INTEGER   CCMPFormatErrors;
    LARGE_INTEGER   CCMPReplays;
    LARGE_INTEGER   CCMPDecryptErrors;
    LARGE_INTEGER   FourWayHandshakeFailures;
} NDIS_802_11_STATISTICS, *PNDIS_802_11_STATISTICS;

typedef  ULONG  NDIS_802_11_KEY_INDEX;
typedef long long NDIS_802_11_KEY_RSC;

// Key mapping keys require a BSSID
typedef struct _NDIS_802_11_KEY
{
    ULONG           Length;             // Length of this structure
    ULONG           KeyIndex;
    ULONG           KeyLength;          // length of key in bytes
    NDIS_802_11_MAC_ADDRESS BSSID;
	char			Reserved[6];		// BCM - added for explicit alignment
    NDIS_802_11_KEY_RSC KeyRSC;
    UCHAR           KeyMaterial[1];     // variable length depending on above field
} NDIS_802_11_KEY, *PNDIS_802_11_KEY;

typedef struct _NDIS_802_11_REMOVE_KEY
{
    ULONG           Length;             // Length of this structure
    ULONG           KeyIndex;
    NDIS_802_11_MAC_ADDRESS BSSID;
} NDIS_802_11_REMOVE_KEY, *PNDIS_802_11_REMOVE_KEY;

typedef struct _NDIS_802_11_WEP
{
    ULONG           Length;             // Length of this structure
    ULONG           KeyIndex;           // 0 is the per-client key, 1-N are the
                                        // global keys
    ULONG           KeyLength;          // length of key in bytes
    UCHAR           KeyMaterial[1];     // variable length depending on above field
} NDIS_802_11_WEP, *PNDIS_802_11_WEP;

typedef enum _NDIS_802_11_NETWORK_INFRASTRUCTURE
{
    Ndis802_11IBSS,
    Ndis802_11Infrastructure,
    Ndis802_11AutoUnknown,
    Ndis802_11InfrastructureMax         // Not a real value, defined as upper bound
} NDIS_802_11_NETWORK_INFRASTRUCTURE, *PNDIS_802_11_NETWORK_INFRASTRUCTURE;

typedef enum _NDIS_802_11_AUTHENTICATION_MODE
{
    Ndis802_11AuthModeOpen,
    Ndis802_11AuthModeShared,
    Ndis802_11AuthModeAutoSwitch,
    Ndis802_11AuthModeWPA,
    Ndis802_11AuthModeWPAPSK,
    Ndis802_11AuthModeWPANone,
    Ndis802_11AuthModeWPA2,
    Ndis802_11AuthModeWPA2PSK,
    Ndis802_11AuthModeMax               // Not a real mode, defined as upper bound
} NDIS_802_11_AUTHENTICATION_MODE, *PNDIS_802_11_AUTHENTICATION_MODE;

typedef  UCHAR   NDIS_802_11_RATES[8];  // Set of 8 data rates
typedef UCHAR   NDIS_802_11_RATES_EX[16];  // Set of 16 data rates

typedef struct _NDIS_802_11_SSID
{
    ULONG   SsidLength;         // length of SSID field below, in bytes;
                                // this can be zero.
    UCHAR   Ssid[32];           // SSID information field
} NDIS_802_11_SSID, *PNDIS_802_11_SSID;

typedef struct _NDIS_WLAN_BSSID
{
    ULONG                               Length;             // Length of this structure
    NDIS_802_11_MAC_ADDRESS             MacAddress;         // BSSID
    UCHAR                               Reserved[2];
    NDIS_802_11_SSID                    Ssid;               // SSID
    ULONG                               Privacy;            // WEP encryption requirement
    NDIS_802_11_RSSI                    Rssi;               // receive signal
                                                            // strength in dBm
    NDIS_802_11_NETWORK_TYPE            NetworkTypeInUse;
    NDIS_802_11_CONFIGURATION           Configuration;
    NDIS_802_11_NETWORK_INFRASTRUCTURE  InfrastructureMode;
    NDIS_802_11_RATES                   SupportedRates;
} NDIS_WLAN_BSSID, *PNDIS_WLAN_BSSID;

typedef struct _NDIS_802_11_BSSID_LIST
{
    ULONG           NumberOfItems;      // in list below, at least 1
    NDIS_WLAN_BSSID Bssid[1];
} NDIS_802_11_BSSID_LIST, *PNDIS_802_11_BSSID_LIST;

// Added Capabilities, IELength and IEs for each BSSID
typedef struct _NDIS_WLAN_BSSID_EX
{
    ULONG                               Length;             // Length of this structure
    NDIS_802_11_MAC_ADDRESS             MacAddress;         // BSSID
    UCHAR                               Reserved[2];
    NDIS_802_11_SSID                    Ssid;               // SSID
    ULONG                               Privacy;            // WEP encryption requirement
    NDIS_802_11_RSSI                    Rssi;               // receive signal
                                                            // strength in dBm
    NDIS_802_11_NETWORK_TYPE            NetworkTypeInUse;
    NDIS_802_11_CONFIGURATION           Configuration;
    NDIS_802_11_NETWORK_INFRASTRUCTURE  InfrastructureMode;
    NDIS_802_11_RATES_EX                SupportedRates;
    ULONG                               IELength;
    UCHAR                               IEs[1];
} NDIS_WLAN_BSSID_EX, *PNDIS_WLAN_BSSID_EX;

typedef struct _NDIS_802_11_BSSID_LIST_EX
{
	ULONG           NumberOfItems;      // in list below, at least 1
	NDIS_WLAN_BSSID_EX Bssid[1];
} NDIS_802_11_BSSID_LIST_EX, *PNDIS_802_11_BSSID_LIST_EX;

typedef struct _NDIS_802_11_VARIABLE_IEs
{
	UCHAR ElementID;
	UCHAR Length;	// Number of bytes in data field
	UCHAR data[1];
} NDIS_802_11_VARIABLE_IEs, *PNDIS_802_11_VARIABLE_IEs;

typedef  ULONG   NDIS_802_11_FRAGMENTATION_THRESHOLD;

typedef  ULONG   NDIS_802_11_RTS_THRESHOLD;

typedef  ULONG   NDIS_802_11_ANTENNA;

typedef enum _NDIS_802_11_PRIVACY_FILTER
{
    Ndis802_11PrivFilterAcceptAll,
    Ndis802_11PrivFilter8021xWEP
} NDIS_802_11_PRIVACY_FILTER, *PNDIS_802_11_PRIVACY_FILTER;

// Added new encryption types
// Also aliased typedef to new name
typedef enum _NDIS_802_11_WEP_STATUS
{
    Ndis802_11WEPEnabled,
    Ndis802_11Encryption1Enabled = Ndis802_11WEPEnabled,
    Ndis802_11WEPDisabled,
    Ndis802_11EncryptionDisabled = Ndis802_11WEPDisabled,
    Ndis802_11WEPKeyAbsent,
    Ndis802_11Encryption1KeyAbsent = Ndis802_11WEPKeyAbsent,
    Ndis802_11WEPNotSupported,
    Ndis802_11EncryptionNotSupported = Ndis802_11WEPNotSupported,
    Ndis802_11Encryption2Enabled,
    Ndis802_11Encryption2KeyAbsent,
    Ndis802_11Encryption3Enabled,
    Ndis802_11Encryption3KeyAbsent
} NDIS_802_11_WEP_STATUS, *PNDIS_802_11_WEP_STATUS,
  NDIS_802_11_ENCRYPTION_STATUS, *PNDIS_802_11_ENCRYPTION_STATUS;

typedef enum _NDIS_802_11_RELOAD_DEFAULTS
{
    Ndis802_11ReloadWEPKeys
} NDIS_802_11_RELOAD_DEFAULTS, *PNDIS_802_11_RELOAD_DEFAULTS;

#define NDIS_802_11_AI_REQFI_CAPABILITIES      1
#define NDIS_802_11_AI_REQFI_LISTENINTERVAL    2
#define NDIS_802_11_AI_REQFI_CURRENTAPADDRESS  4

#define NDIS_802_11_AI_RESFI_CAPABILITIES      1
#define NDIS_802_11_AI_RESFI_STATUSCODE        2
#define NDIS_802_11_AI_RESFI_ASSOCIATIONID     4

typedef struct _NDIS_802_11_ASSOCIATION_INFORMATION
{
    ULONG Length;
    USHORT AvailableRequestFixedIEs;
    struct _NDIS_802_11_AI_REQFI {
		USHORT Capabilities;
		USHORT ListenInterval;
                NDIS_802_11_MAC_ADDRESS  CurrentAPAddress;
    } RequestFixedIEs;
    ULONG RequestIELength;
    ULONG OffsetRequestIEs;
    USHORT AvailableResponseFixedIEs;
    struct _NDIS_802_11_AI_RESFI {
		USHORT Capabilities;
		USHORT StatusCode;
		USHORT AssociationId;
    } ResponseFixedIEs;
    ULONG ResponseIELength;
    ULONG OffsetResponseIEs;
} NDIS_802_11_ASSOCIATION_INFORMATION, *PNDIS_802_11_ASSOCIATION_INFORMATION;

typedef struct _NDIS_802_11_AUTHENTICATION_EVENT
{
    NDIS_802_11_STATUS_INDICATION       Status;
    NDIS_802_11_AUTHENTICATION_REQUEST  Request[1];
} NDIS_802_11_AUTHENTICATION_EVENT, *PNDIS_802_11_AUTHENTICATION_EVENT;

typedef struct _NDIS_802_11_TEST
{
	ULONG Length;
	ULONG Type;
	union {
        NDIS_802_11_AUTHENTICATION_EVENT AuthenticationEvent;
		NDIS_802_11_RSSI RssiTrigger;
	};
} NDIS_802_11_TEST, *PNDIS_802_11_TEST;

// PMKID Structures
typedef UCHAR   NDIS_802_11_PMKID_VALUE[16];

typedef struct _BSSID_INFO
{
    NDIS_802_11_MAC_ADDRESS BSSID;
    NDIS_802_11_PMKID_VALUE PMKID;
} BSSID_INFO, *PBSSID_INFO;

typedef struct _NDIS_802_11_PMKID
{
    ULONG Length;
    ULONG BSSIDInfoCount;
    BSSID_INFO BSSIDInfo[1];
} NDIS_802_11_PMKID, *PNDIS_802_11_PMKID;

typedef struct _NDIS_802_11_AUTH_ENCRYPTION
{
	NDIS_802_11_AUTHENTICATION_MODE AuthModeSupported;
	NDIS_802_11_ENCRYPTION_STATUS EncryptStatusSupported;
} NDIS_802_11_AUTH_ENCRYPTION, *PNDIS_802_11_AUTH_ENCRYPTION;

typedef struct _NDIS_802_11_CAPABILITY
{
    ULONG Length;
    ULONG Version;
    ULONG NoOfPMKIDs;
    ULONG NoOfAuthEncryptPairsSupported;
    NDIS_802_11_AUTH_ENCRYPTION AuthEncryptionSupported[1];
} NDIS_802_11_CAPABILITY, *PNDIS_802_11_CAPABILITY;

//
// Medium the Ndis Driver is running on (OID_GEN_MEDIA_SUPPORTED/ OID_GEN_MEDIA_IN_USE).
//
typedef enum _NDIS_MEDIUM
{
    NdisMedium802_3,
    NdisMedium802_5,
    NdisMediumFddi,
    NdisMediumWan,
    NdisMediumLocalTalk,
    NdisMediumDix,              // defined for convenience, not a real medium
    NdisMediumArcnetRaw,
    NdisMediumArcnet878_2,
    NdisMediumAtm,
    NdisMediumWirelessWan,
    NdisMediumIrda,
    NdisMediumBpc,
    NdisMediumCoWan,
    NdisMedium1394,
    NdisMediumMax               // Not a real medium, defined as an upper-bound
} NDIS_MEDIUM, *PNDIS_MEDIUM;

//
// Physical Medium Type definitions. Used with OID_GEN_PHYSICAL_MEDIUM.
//
typedef enum _NDIS_PHYSICAL_MEDIUM
{
    NdisPhysicalMediumUnspecified,
    NdisPhysicalMediumWirelessLan,
    NdisPhysicalMediumCableModem,
    NdisPhysicalMediumPhoneLine,
    NdisPhysicalMediumPowerLine,
    NdisPhysicalMediumDSL,      // includes ADSL and UADSL (G.Lite)
    NdisPhysicalMediumFibreChannel,
    NdisPhysicalMedium1394,
    NdisPhysicalMediumWirelessWan,
    NdisPhysicalMediumMax       // Not a real physical type, defined as an upper-bound
} NDIS_PHYSICAL_MEDIUM, *PNDIS_PHYSICAL_MEDIUM;

//
// Hardware status codes (OID_GEN_HARDWARE_STATUS).
//
typedef enum _NDIS_HARDWARE_STATUS
{
    NdisHardwareStatusReady,
    NdisHardwareStatusInitializing,
    NdisHardwareStatusReset,
    NdisHardwareStatusClosing,
    NdisHardwareStatusNotReady
} NDIS_HARDWARE_STATUS, *PNDIS_HARDWARE_STATUS;

//
// Defines the state of the LAN media
//
typedef enum _NDIS_MEDIA_STATE
{
    NdisMediaStateConnected,
    NdisMediaStateDisconnected
} NDIS_MEDIA_STATE, *PNDIS_MEDIA_STATE;

#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct _GUID {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[ 8 ];
} GUID;
#endif

#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        const GUID name \
                = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

#define OID_GEN_SUPPORTED_GUIDS                 0x00010117

//
//  Structure to be used for OID_GEN_SUPPORTED_GUIDS.
//  This structure describes an OID to GUID mapping.
//  Or a Status to GUID mapping.
//  When ndis receives a request for a give GUID it will
//  query the miniport with the supplied OID.
//
typedef struct _NDIS_GUID
{
    GUID            Guid;
    union
    {
        NDIS_OID    Oid;
        NDIS_STATUS Status;
    };
    ULONG       Size;               //  Size of the data element. If the GUID
                                    //  represents an array then this is the
                                    //  size of an element in the array.
                                    //  This is -1 for strings.
    ULONG       Flags;
} NDIS_GUID, *PNDIS_GUID;

#define fNDIS_GUID_TO_OID           0x00000001
#define fNDIS_GUID_TO_STATUS        0x00000002
#define fNDIS_GUID_ANSI_STRING      0x00000004
#define fNDIS_GUID_UNICODE_STRING   0x00000008
#define fNDIS_GUID_ARRAY            0x00000010
#define fNDIS_GUID_ALLOW_READ       0x00000020
#define fNDIS_GUID_ALLOW_WRITE      0x00000040

//
// Possible registry configuration value data types
//
typedef enum _NDIS_PARAMETER_TYPE
{
    NdisParameterInteger,
    NdisParameterHexInteger,
    NdisParameterString,
    NdisParameterMultiString,
    NdisParameterBinary
} NDIS_PARAMETER_TYPE, *PNDIS_PARAMETER_TYPE;

typedef struct _NDIS_STRING {
  USHORT  Length;
  USHORT  MaximumLength;
  char *Buffer;
} NDIS_STRING, *PNDIS_STRING;

typedef struct _NDIS_CONFIGURATION_PARAMETER {
    NDIS_PARAMETER_TYPE ParameterType;
    union {
        ULONG IntegerData;
        NDIS_STRING StringData;
    } ParameterData;
} NDIS_CONFIGURATION_PARAMETER, *PNDIS_CONFIGURATION_PARAMETER;

#else /* EXT_STA */

/* inlined from wlantypes.h */

typedef enum _DOT11_BSS_TYPE {
    dot11_BSS_type_infrastructure = 1,
    dot11_BSS_type_independent = 2,
    dot11_BSS_type_any = 3
} DOT11_BSS_TYPE, * PDOT11_BSS_TYPE;

#define DOT11_SSID_MAX_LENGTH   32      // 32 bytes
typedef struct _DOT11_SSID {
    ULONG uSSIDLength;
    UCHAR ucSSID[DOT11_SSID_MAX_LENGTH];
} DOT11_SSID, * PDOT11_SSID;


// DOT11_AUTH_ALGO_LIST
typedef enum _DOT11_AUTH_ALGORITHM {
    DOT11_AUTH_ALGO_80211_OPEN = 1,
    DOT11_AUTH_ALGO_80211_SHARED_KEY = 2,
    DOT11_AUTH_ALGO_WPA = 3,
    DOT11_AUTH_ALGO_WPA_PSK = 4,
    DOT11_AUTH_ALGO_WPA_NONE = 5,               // used in NatSTA only
    DOT11_AUTH_ALGO_RSNA = 6,
    DOT11_AUTH_ALGO_RSNA_PSK = 7,
    DOT11_AUTH_ALGO_IHV_START = 0x80000000,
    DOT11_AUTH_ALGO_IHV_END = 0xffffffff
} DOT11_AUTH_ALGORITHM, * PDOT11_AUTH_ALGORITHM;

#define DOT11_AUTH_ALGORITHM_OPEN_SYSTEM        DOT11_AUTH_ALGO_80211_OPEN
#define DOT11_AUTH_ALGORITHM_SHARED_KEY         DOT11_AUTH_ALGO_80211_SHARED_KEY
#define DOT11_AUTH_ALGORITHM_WPA                DOT11_AUTH_ALGO_WPA
#define DOT11_AUTH_ALGORITHM_WPA_PSK            DOT11_AUTH_ALGO_WPA_PSK
#define DOT11_AUTH_ALGORITHM_WPA_NONE           DOT11_AUTH_ALGO_WPA_NONE
#define DOT11_AUTH_ALGORITHM_RSNA               DOT11_AUTH_ALGO_RSNA
#define DOT11_AUTH_ALGORITHM_RSNA_PSK           DOT11_AUTH_ALGO_RSNA_PSK

// Cipher algorithm Ids (for little endian platform)
typedef enum _DOT11_CIPHER_ALGORITHM {
    DOT11_CIPHER_ALGO_NONE = 0x00,
    DOT11_CIPHER_ALGO_WEP40 = 0x01,
    DOT11_CIPHER_ALGO_TKIP = 0x02,
    DOT11_CIPHER_ALGO_CCMP = 0x04,
    DOT11_CIPHER_ALGO_WEP104 = 0x05,
    DOT11_CIPHER_ALGO_BIP = 0x06,
    DOT11_CIPHER_ALGO_WPA_USE_GROUP = 0x100,
    DOT11_CIPHER_ALGO_RSN_USE_GROUP = 0x100,
    DOT11_CIPHER_ALGO_WEP = 0x101,
    DOT11_CIPHER_ALGO_IHV_START = 0x80000000,
    DOT11_CIPHER_ALGO_IHV_END = 0xffffffff
} DOT11_CIPHER_ALGORITHM, * PDOT11_CIPHER_ALGORITHM;

typedef struct DOT11_AUTH_CIPHER_PAIR {
    DOT11_AUTH_ALGORITHM AuthAlgoId;
    DOT11_CIPHER_ALGORITHM CipherAlgoId;
} DOT11_AUTH_CIPHER_PAIR, * PDOT11_AUTH_CIPHER_PAIR;

/* inlined from ntddndis.h */

//
// NDIS Object Types used in NDIS_OBJECT_HEADER
//
#define NDIS_OBJECT_TYPE_DEFAULT                            0x80    // used when object type is implicit in the API call
#define NDIS_OBJECT_TYPE_MINIPORT_INIT_PARAMETERS           0x81    // used by NDIS in NDIS_MINIPORT_INIT_PARAMETERS
#define NDIS_OBJECT_TYPE_SG_DMA_DESCRIPTION                 0x83    // used by miniport drivers in NDIS_SG_DMA_DESCRIPTION
#define NDIS_OBJECT_TYPE_MINIPORT_INTERRUPT                 0x84    // used by miniport drivers in NDIS_MINIPORT_INTERRUPT_EX
#define NDIS_OBJECT_TYPE_DEVICE_OBJECT_ATTRIBUTES           0x85    // used by miniport or filter drivers in NDIS_DEVICE_OBJECT_ATTRIBUTES
#define NDIS_OBJECT_TYPE_BIND_PARAMETERS                    0x86    // used by NDIS in NDIS_BIND_PARAMETERS
#define NDIS_OBJECT_TYPE_OPEN_PARAMETERS                    0x87    // used by protocols in NDIS_OPEN_PARAMETERS
#define NDIS_OBJECT_TYPE_RSS_CAPABILITIES                   0x88    // used by miniport in NDIS_RECEIVE_SCALE_CAPABILITIES
#define NDIS_OBJECT_TYPE_RSS_PARAMETERS                     0x89    // used by miniport and protocol in NDIS_RECEIVE_SCALE_PARAMETERS
#define NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS    0x8A
#define NDIS_OBJECT_TYPE_FILTER_DRIVER_CHARACTERISTICS      0x8B
#define NDIS_OBJECT_TYPE_FILTER_PARTIAL_CHARACTERISTICS     0x8C
#define NDIS_OBJECT_TYPE_FILTER_ATTRIBUTES                  0x8D
#define NDIS_OBJECT_TYPE_CLIENT_CHIMNEY_OFFLOAD_GENERIC_CHARACTERISTICS     0x8E
#define NDIS_OBJECT_TYPE_PROVIDER_CHIMNEY_OFFLOAD_GENERIC_CHARACTERISTICS   0x8F
#define NDIS_OBJECT_TYPE_CO_PROTOCOL_CHARACTERISTICS        0x90
#define NDIS_OBJECT_TYPE_CO_MINIPORT_CHARACTERISTICS        0x91
#define NDIS_OBJECT_TYPE_MINIPORT_PNP_CHARACTERISTICS       0x92
#define NDIS_OBJECT_TYPE_CLIENT_CHIMNEY_OFFLOAD_CHARACTERISTICS     0x93
#define NDIS_OBJECT_TYPE_PROVIDER_CHIMNEY_OFFLOAD_CHARACTERISTICS   0x94
#define NDIS_OBJECT_TYPE_PROTOCOL_DRIVER_CHARACTERISTICS    0x95
#define NDIS_OBJECT_TYPE_REQUEST_EX                         0x96
#define NDIS_OBJECT_TYPE_OID_REQUEST                        0x96
#define NDIS_OBJECT_TYPE_TIMER_CHARACTERISTICS              0x97
#define NDIS_OBJECT_TYPE_STATUS_INDICATION                  0x98
#define NDIS_OBJECT_TYPE_FILTER_ATTACH_PARAMETERS           0x99
#define NDIS_OBJECT_TYPE_FILTER_PAUSE_PARAMETERS            0x9A
#define NDIS_OBJECT_TYPE_FILTER_RESTART_PARAMETERS          0x9B
#define NDIS_OBJECT_TYPE_PORT_CHARACTERISTICS               0x9C
#define NDIS_OBJECT_TYPE_PORT_STATE                         0x9D
#define NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES       0x9E
#define NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES            0x9F
#define NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES            0xA0
#define NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_NATIVE_802_11_ATTRIBUTES      0xA1
#define NDIS_OBJECT_TYPE_RESTART_GENERAL_ATTRIBUTES                     0xA2
#define NDIS_OBJECT_TYPE_PROTOCOL_RESTART_PARAMETERS                    0xA3
#define NDIS_OBJECT_TYPE_MINIPORT_ADD_DEVICE_REGISTRATION_ATTRIBUTES    0xA4
#define NDIS_OBJECT_TYPE_CO_CALL_MANAGER_OPTIONAL_HANDLERS              0xA5
#define NDIS_OBJECT_TYPE_CO_CLIENT_OPTIONAL_HANDLERS                    0xA6
#define NDIS_OBJECT_TYPE_OFFLOAD                                        0xA7
#define NDIS_OBJECT_TYPE_OFFLOAD_ENCAPSULATION                          0xA8
#define NDIS_OBJECT_TYPE_CONFIGURATION_OBJECT                           0xA9
#define NDIS_OBJECT_TYPE_DRIVER_WRAPPER_OBJECT                          0xAA
#define NDIS_OBJECT_TYPE_RESERVED                                       0xAB
#define NDIS_OBJECT_TYPE_NSI_NETWORK_RW_STRUCT                          0xAC
#define NDIS_OBJECT_TYPE_NSI_COMPARTMENT_RW_STRUCT                      0xAD
#define NDIS_OBJECT_TYPE_NSI_INTERFACE_PERSIST_RW_STRUCT                0xAE

typedef struct _NDIS_OBJECT_HEADER
{
    UCHAR   Type;
    UCHAR   Revision;
    USHORT  Size;
} NDIS_OBJECT_HEADER, *PNDIS_OBJECT_HEADER;

#define NDIS_802_11_LENGTH_SSID         32
#define NDIS_802_11_LENGTH_RATES        8
#define NDIS_802_11_LENGTH_RATES_EX     16

typedef UCHAR   NDIS_802_11_MAC_ADDRESS[6];

typedef UCHAR   NDIS_802_11_RATES[NDIS_802_11_LENGTH_RATES];        // Set of 8 data rates
typedef UCHAR   NDIS_802_11_RATES_EX[NDIS_802_11_LENGTH_RATES_EX];  // Set of 16 data rates

typedef struct _NDIS_802_11_SSID
{
    ULONG   SsidLength;         // length of SSID field below, in bytes;
                                // this can be zero.
    UCHAR   Ssid[NDIS_802_11_LENGTH_SSID];           // SSID information field
} NDIS_802_11_SSID, *PNDIS_802_11_SSID;

// Added new types for OFDM 5G and 2.4G
typedef enum _NDIS_802_11_NETWORK_TYPE
{
    Ndis802_11FH,
    Ndis802_11DS,
    Ndis802_11OFDM5,
    Ndis802_11OFDM24,
    Ndis802_11Automode,
    Ndis802_11NetworkTypeMax    // not a real type, defined as an upper bound
} NDIS_802_11_NETWORK_TYPE, *PNDIS_802_11_NETWORK_TYPE;

typedef struct _NDIS_802_11_NETWORK_TYPE_LIST
{
    ULONG                       NumberOfItems;  // in list below, at least 1
    NDIS_802_11_NETWORK_TYPE    NetworkType [1];
} NDIS_802_11_NETWORK_TYPE_LIST, *PNDIS_802_11_NETWORK_TYPE_LIST;

typedef enum _NDIS_802_11_POWER_MODE
{
    Ndis802_11PowerModeCAM,
    Ndis802_11PowerModeMAX_PSP,
    Ndis802_11PowerModeFast_PSP,
    Ndis802_11PowerModeMax      // not a real mode, defined as an upper bound
} NDIS_802_11_POWER_MODE, *PNDIS_802_11_POWER_MODE;

//
// Received Signal Strength Indication
//
typedef LONG   NDIS_802_11_RSSI;           // in dBm

typedef struct _NDIS_802_11_CONFIGURATION_FH
{
    ULONG           Length;             // Length of structure
    ULONG           HopPattern;         // As defined by 802.11, MSB set
    ULONG           HopSet;             // to one if non-802.11
    ULONG           DwellTime;          // units are Kusec
} NDIS_802_11_CONFIGURATION_FH, *PNDIS_802_11_CONFIGURATION_FH;

typedef struct _NDIS_802_11_CONFIGURATION
{
    ULONG           Length;             // Length of structure
    ULONG           BeaconPeriod;       // units are Kusec
    ULONG           ATIMWindow;         // units are Kusec
    ULONG           DSConfig;           // Frequency, units are kHz
    NDIS_802_11_CONFIGURATION_FH    FHConfig;
} NDIS_802_11_CONFIGURATION, *PNDIS_802_11_CONFIGURATION;

typedef enum _NDIS_802_11_NETWORK_INFRASTRUCTURE
{
    Ndis802_11IBSS,
    Ndis802_11Infrastructure,
    Ndis802_11AutoUnknown,
    Ndis802_11InfrastructureMax         // Not a real value, defined as upper bound
} NDIS_802_11_NETWORK_INFRASTRUCTURE, *PNDIS_802_11_NETWORK_INFRASTRUCTURE;

// Added Capabilities, IELength and IEs for each BSSID
typedef struct _NDIS_WLAN_BSSID_EX
{
    ULONG                               Length;             // Length of this structure
    NDIS_802_11_MAC_ADDRESS             MacAddress;         // BSSID
    UCHAR                               Reserved[2];
    NDIS_802_11_SSID                    Ssid;               // SSID
    ULONG                               Privacy;            // WEP encryption requirement
    NDIS_802_11_RSSI                    Rssi;               // receive signal
                                                            // strength in dBm
    NDIS_802_11_NETWORK_TYPE            NetworkTypeInUse;
    NDIS_802_11_CONFIGURATION           Configuration;
    NDIS_802_11_NETWORK_INFRASTRUCTURE  InfrastructureMode;
    NDIS_802_11_RATES_EX                SupportedRates;
    ULONG                               IELength;
    UCHAR                               IEs[1];
} NDIS_WLAN_BSSID_EX, *PNDIS_WLAN_BSSID_EX;

typedef struct _NDIS_802_11_BSSID_LIST_EX
{
    ULONG           NumberOfItems;      // in list below, at least 1
    NDIS_WLAN_BSSID_EX Bssid[1];
} NDIS_802_11_BSSID_LIST_EX, *PNDIS_802_11_BSSID_LIST_EX;


/* inlined from windot11.h */

typedef struct _DOT11_MAC_ADDRESS {
    UCHAR ucDot11MacAddress[6];
} DOT11_MAC_ADDRESS, * PDOT11_MAC_ADDRESS;

// A list of DOT11_MAC_ADDRESS
typedef struct DOT11_BSSID_LIST {
    #define DOT11_BSSID_LIST_REVISION_1  1
    NDIS_OBJECT_HEADER Header;
    ULONG uNumOfEntries;
    ULONG uTotalNumOfEntries;
    DOT11_MAC_ADDRESS BSSIDs[1];
} DOT11_BSSID_LIST, * PDOT11_BSSID_LIST;

typedef enum _DOT11_PHY_TYPE {
    dot11_phy_type_unknown = 0,
    dot11_phy_type_any = dot11_phy_type_unknown,
    dot11_phy_type_fhss = 1,
    dot11_phy_type_dsss = 2,
    dot11_phy_type_irbaseband = 3,
    dot11_phy_type_ofdm = 4,
    dot11_phy_type_hrdsss = 5,
    dot11_phy_type_erp = 6,
    dot11_phy_type_ht = 7,
    dot11_phy_type_vht = 8,
    dot11_phy_type_IHV_start = 0x80000000,
    dot11_phy_type_IHV_end = 0xffffffff
} DOT11_PHY_TYPE, * PDOT11_PHY_TYPE;

#define DOT11_RATE_SET_MAX_LENGTH               126 // 126 bytes
typedef struct _DOT11_RATE_SET {
    ULONG uRateSetLength;
    UCHAR ucRateSet[DOT11_RATE_SET_MAX_LENGTH];
} DOT11_RATE_SET, * PDOT11_RATE_SET;

typedef UCHAR DOT11_COUNTRY_OR_REGION_STRING[3];
typedef DOT11_COUNTRY_OR_REGION_STRING * PDOT11_COUNTRY_OR_REGION_STRING;


#define OID_DOT11_NDIS_START                        0x0D010300

#define NWF_MANDATORY_OID       (0x01U)
#define NWF_OPTIONAL_OID        (0x02U)

#define NWF_OPERATIONAL_OID     (0x01U)
#define NWF_STATISTICS_OID      (0x02U)

#define NWF_DEFINE_OID(Seq,o,m)     ((0x0E000000U) | ((o) << 16) | ((m) << 8) | (Seq))

//
// Offload Capability OIDs
//

#define OID_DOT11_OFFLOAD_CAPABILITY                (OID_DOT11_NDIS_START + 0)
    // Capability flags
    #define DOT11_HW_WEP_SUPPORTED_TX               0x00000001
    #define DOT11_HW_WEP_SUPPORTED_RX               0x00000002
    #define DOT11_HW_FRAGMENTATION_SUPPORTED        0x00000004
    #define DOT11_HW_DEFRAGMENTATION_SUPPORTED      0x00000008
    #define DOT11_HW_MSDU_AUTH_SUPPORTED_TX         0x00000010
    #define DOT11_HW_MSDU_AUTH_SUPPORTED_RX         0x00000020
    // WEP Algorithm flags
    #define DOT11_CONF_ALGO_WEP_RC4                 0x00000001  // WEP RC4
    #define DOT11_CONF_ALGO_TKIP                    0x00000002
    // Integrity Algorithm flags
    #define DOT11_AUTH_ALGO_MICHAEL                 0x00000001  // Michael
    typedef struct _DOT11_OFFLOAD_CAPABILITY {
        ULONG uReserved;
        ULONG uFlags;
        ULONG uSupportedWEPAlgorithms;
        ULONG uNumOfReplayWindows;
        ULONG uMaxWEPKeyMappingLength;
        ULONG uSupportedAuthAlgorithms;
        ULONG uMaxAuthKeyMappingLength;
    } DOT11_OFFLOAD_CAPABILITY, * PDOT11_OFFLOAD_CAPABILITY;

#define OID_DOT11_CURRENT_OFFLOAD_CAPABILITY        (OID_DOT11_NDIS_START + 1)
    typedef struct _DOT11_CURRENT_OFFLOAD_CAPABILITY {
        ULONG uReserved;
        ULONG uFlags;
    } DOT11_CURRENT_OFFLOAD_CAPABILITY, * PDOT11_CURRENT_OFFLOAD_CAPABILITY;


//
// WEP Offload
//

#define OID_DOT11_WEP_OFFLOAD                       (OID_DOT11_NDIS_START + 2)
    typedef enum _DOT11_OFFLOAD_TYPE {
        dot11_offload_type_wep = 1,
        dot11_offload_type_auth = 2
    } DOT11_OFFLOAD_TYPE, * PDOT11_OFFLOAD_TYPE;
    typedef struct _DOT11_IV48_COUNTER {
        ULONG uIV32Counter;
        USHORT usIV16Counter;
    } DOT11_IV48_COUNTER, * PDOT11_IV48_COUNTER;
    typedef struct _DOT11_WEP_OFFLOAD {
        ULONG uReserved;
        HANDLE hOffloadContext;
        HANDLE hOffload;
        DOT11_OFFLOAD_TYPE dot11OffloadType;
        ULONG dwAlgorithm;
        BOOLEAN bRowIsOutbound;
        BOOLEAN bUseDefault;
        ULONG uFlags;
        UCHAR ucMacAddress[6];
        ULONG uNumOfRWsOnPeer;
        ULONG uNumOfRWsOnMe;
        DOT11_IV48_COUNTER dot11IV48Counters[16];
        USHORT usDot11RWBitMaps[16];
        USHORT usKeyLength;
        UCHAR ucKey[1];             // Must be the last field.
    } DOT11_WEP_OFFLOAD, * PDOT11_WEP_OFFLOAD;

#define OID_DOT11_WEP_UPLOAD                        (OID_DOT11_NDIS_START + 3)
    typedef struct _DOT11_WEP_UPLOAD {
        ULONG uReserved;
        DOT11_OFFLOAD_TYPE dot11OffloadType;
        HANDLE hOffload;
        ULONG uNumOfRWsUsed;
        DOT11_IV48_COUNTER dot11IV48Counters[16];
        USHORT usDot11RWBitMaps[16];
    } DOT11_WEP_UPLOAD, * PDOT11_WEP_UPLOAD;

#define OID_DOT11_DEFAULT_WEP_OFFLOAD               (OID_DOT11_NDIS_START + 4)
    typedef enum _DOT11_KEY_DIRECTION {
        dot11_key_direction_both = 1,
        dot11_key_direction_inbound = 2,
        dot11_key_direction_outbound = 3
    } DOT11_KEY_DIRECTION, * PDOT11_KEY_DIRECTION;
    typedef struct _DOT11_DEFAULT_WEP_OFFLOAD {
        ULONG uReserved;
        HANDLE hOffloadContext;
        HANDLE hOffload;
        ULONG  dwIndex;
        DOT11_OFFLOAD_TYPE dot11OffloadType;
        ULONG dwAlgorithm;
        ULONG uFlags;
        DOT11_KEY_DIRECTION dot11KeyDirection;
        UCHAR ucMacAddress[6];
        ULONG uNumOfRWsOnMe;
        DOT11_IV48_COUNTER dot11IV48Counters[16];
        USHORT usDot11RWBitMaps[16];
        USHORT usKeyLength;
        UCHAR ucKey[1];             // Must be the last field.
    } DOT11_DEFAULT_WEP_OFFLOAD, * PDOT11_DEFAULT_WEP_OFFLOAD;

#define OID_DOT11_DEFAULT_WEP_UPLOAD                (OID_DOT11_NDIS_START + 5)
    typedef struct _DOT11_DEFAULT_WEP_UPLOAD {
        ULONG uReserved;
        DOT11_OFFLOAD_TYPE dot11OffloadType;
        HANDLE hOffload;
        ULONG uNumOfRWsUsed;
        DOT11_IV48_COUNTER dot11IV48Counters[16];
        USHORT usDot11RWBitMaps[16];
    } DOT11_DEFAULT_WEP_UPLOAD, * PDOT11_DEFAULT_WEP_UPLOAD;

//
// Fragmentation/Defragmentation Offload
//

#define OID_DOT11_MPDU_MAX_LENGTH                   (OID_DOT11_NDIS_START + 6)
    // ULONG (in bytes)

//
// 802.11 Configuration OIDs
//

//
// OIDs for Mandatory Functions
//

#define OID_DOT11_OPERATION_MODE_CAPABILITY         (OID_DOT11_NDIS_START + 7)
    #define DOT11_OPERATION_MODE_UNKNOWN            0x00000000
    #define DOT11_OPERATION_MODE_STATION            0x00000001
    #define DOT11_OPERATION_MODE_AP                 0x00000002
    #define DOT11_OPERATION_MODE_EXTENSIBLE_STATION 0x00000004
    #define DOT11_OPERATION_MODE_NETWORK_MONITOR    0x80000000
    typedef struct _DOT11_OPERATION_MODE_CAPABILITY {
        ULONG uReserved;
        ULONG uMajorVersion;
        ULONG uMinorVersion;
        ULONG uNumOfTXBuffers;
        ULONG uNumOfRXBuffers;
        ULONG uOpModeCapability;
    } DOT11_OPERATION_MODE_CAPABILITY, * PDOT11_OPERATION_MODE_CAPABILITY;

#define OID_DOT11_CURRENT_OPERATION_MODE            (OID_DOT11_NDIS_START + 8)
    typedef struct _DOT11_CURRENT_OPERATION_MODE {
        ULONG uReserved;
        ULONG uCurrentOpMode;
    } DOT11_CURRENT_OPERATION_MODE, * PDOT11_CURRENT_OPERATION_MODE;

#define OID_DOT11_CURRENT_PACKET_FILTER             (OID_DOT11_NDIS_START + 9)
    #define DOT11_PACKET_TYPE_DIRECTED_CTRL         0x00000001
    // Indicate all 802.11 unicast control packets.
    #define DOT11_PACKET_TYPE_DIRECTED_MGMT         0x00000002
    // Indicate all 802.11 unicast management packets.
    #define DOT11_PACKET_TYPE_DIRECTED_DATA         0x00000004
    // Indicate all 802.11 unicast data packets.
    #define DOT11_PACKET_TYPE_MULTICAST_CTRL        0x00000008
    // Indicate all 802.11 multicast control packets.
    #define DOT11_PACKET_TYPE_MULTICAST_MGMT        0x00000010
    // Indicate all 802.11 multicast management packets.
    #define DOT11_PACKET_TYPE_MULTICAST_DATA        0x00000020
    // Indicate all 802.11 multicast data packets.
    #define DOT11_PACKET_TYPE_BROADCAST_CTRL        0x00000040
    // Indicate all 802.11 broadcast control packets.
    #define DOT11_PACKET_TYPE_BROADCAST_MGMT        0x00000080
    // Indicate all 802.11 broadcast management packets.
    #define DOT11_PACKET_TYPE_BROADCAST_DATA        0x00000100
    // Indicate all 802.11 broadcast data packets.
    #define DOT11_PACKET_TYPE_PROMISCUOUS_CTRL      0x00000200
    // Move into promiscuous mode and indicate all 802.11 control packets.
    #define DOT11_PACKET_TYPE_PROMISCUOUS_MGMT      0x00000400
    // Move into promiscuous mode and indicate all 802.11 control packets.
    #define DOT11_PACKET_TYPE_PROMISCUOUS_DATA      0x00000800
    // Move into promiscuous mode and indicate all 802.11 control packets.
    #define DOT11_PACKET_TYPE_ALL_MULTICAST_CTRL    0x00001000
    // Indicate all 802.11 multicast control packets.
    #define DOT11_PACKET_TYPE_ALL_MULTICAST_MGMT    0x00002000
    // Indicate all 802.11 multicast management packets.
    #define DOT11_PACKET_TYPE_ALL_MULTICAST_DATA    0x00004000
    // Indicate all 802.11 multicast data packets.
    #define DOT11_PACKET_TYPE_RESERVED  (~(             \
                DOT11_PACKET_TYPE_DIRECTED_CTRL |       \
                DOT11_PACKET_TYPE_DIRECTED_MGMT |       \
                DOT11_PACKET_TYPE_DIRECTED_DATA |       \
                DOT11_PACKET_TYPE_MULTICAST_CTRL |      \
                DOT11_PACKET_TYPE_MULTICAST_MGMT |      \
                DOT11_PACKET_TYPE_MULTICAST_DATA |      \
                DOT11_PACKET_TYPE_BROADCAST_CTRL |      \
                DOT11_PACKET_TYPE_BROADCAST_MGMT |      \
                DOT11_PACKET_TYPE_BROADCAST_DATA |      \
                DOT11_PACKET_TYPE_PROMISCUOUS_CTRL |    \
                DOT11_PACKET_TYPE_PROMISCUOUS_MGMT |    \
                DOT11_PACKET_TYPE_PROMISCUOUS_DATA |    \
                DOT11_PACKET_TYPE_ALL_MULTICAST_CTRL |  \
                DOT11_PACKET_TYPE_ALL_MULTICAST_MGMT |  \
                DOT11_PACKET_TYPE_ALL_MULTICAST_DATA |  \
                0                                       \
                ))
    // All the reserved bits

#define OID_DOT11_ATIM_WINDOW                       (OID_DOT11_NDIS_START + 10)
    // ULONG (in TUs)

#define OID_DOT11_SCAN_REQUEST                      (OID_DOT11_NDIS_START + 11)

    typedef enum _DOT11_SCAN_TYPE {
        dot11_scan_type_active = 1,
        dot11_scan_type_passive = 2,
        dot11_scan_type_auto = 3,
        dot11_scan_type_forced = 0x80000000
    } DOT11_SCAN_TYPE, * PDOT11_SCAN_TYPE;
    typedef struct _DOT11_SCAN_REQUEST {
        DOT11_BSS_TYPE dot11BSSType;
        DOT11_MAC_ADDRESS dot11BSSID;
        DOT11_SSID dot11SSID;
        DOT11_SCAN_TYPE dot11ScanType;
        BOOLEAN bRestrictedScan;
        BOOLEAN bUseRequestIE;
        ULONG uRequestIDsOffset;
        ULONG uNumOfRequestIDs;
        ULONG uPhyTypesOffset;
        ULONG uNumOfPhyTypes;
        ULONG uIEsOffset;
        ULONG uIEsLength;
        UCHAR ucBuffer[1];
    } DOT11_SCAN_REQUEST, * PDOT11_SCAN_REQUEST;

    // V2 SCAN REQUEST
    typedef enum _CH_DESCRIPTION_TYPE {
        ch_description_type_logical = 1,
        ch_description_type_center_frequency = 2,
        ch_description_type_phy_specific
    } CH_DESCRIPTION_TYPE, * PCH_DESCRIPTION_TYPE;
    typedef struct _DOT11_PHY_TYPE_INFO {
        DOT11_PHY_TYPE dot11PhyType;
        BOOLEAN bUseParameters;
        ULONG uProbeDelay;
        ULONG uMinChannelTime;
        ULONG uMaxChannelTime;
        CH_DESCRIPTION_TYPE ChDescriptionType;
        ULONG uChannelListSize;
        UCHAR ucChannelListBuffer[1];
    } DOT11_PHY_TYPE_INFO, * PDOT11_PHY_TYPE_INFO;

    typedef struct _DOT11_SCAN_REQUEST_V2 {
        DOT11_BSS_TYPE dot11BSSType;
        DOT11_MAC_ADDRESS dot11BSSID;
        DOT11_SCAN_TYPE dot11ScanType;
        BOOLEAN bRestrictedScan;
        ULONG udot11SSIDsOffset;
        ULONG uNumOfdot11SSIDs;
        BOOLEAN bUseRequestIE;
        ULONG uRequestIDsOffset;
        ULONG uNumOfRequestIDs;
        ULONG uPhyTypeInfosOffset;
        ULONG uNumOfPhyTypeInfos;
        ULONG uIEsOffset;
        ULONG uIEsLength;
        UCHAR ucBuffer[1];
    } DOT11_SCAN_REQUEST_V2, * PDOT11_SCAN_REQUEST_V2;

#define OID_DOT11_CURRENT_PHY_TYPE                  (OID_DOT11_NDIS_START + 12)
    typedef struct DOT11_PHY_TYPE_LIST {
        #define DOT11_PHY_TYPE_LIST_REVISION_1          1
        NDIS_OBJECT_HEADER Header;
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_PHY_TYPE dot11PhyType[1];
    } DOT11_PHY_TYPE_LIST, * PDOT11_PHY_TYPE_LIST;

#define OID_DOT11_JOIN_REQUEST                      (OID_DOT11_NDIS_START + 13)

    // Capability Information Flags - Exactly maps to the bit positions
    // in the Capability Information field of the beacon and probe response frames.
    #define DOT11_CAPABILITY_INFO_ESS               0x0001
    #define DOT11_CAPABILITY_INFO_IBSS              0x0002
    #define DOT11_CAPABILITY_INFO_CF_POLLABLE       0x0004
    #define DOT11_CAPABILITY_INFO_CF_POLL_REQ       0x0008
    #define DOT11_CAPABILITY_INFO_PRIVACY           0x0010
    #define DOT11_CAPABILITY_SHORT_PREAMBLE         0x0020
    #define DOT11_CAPABILITY_PBCC                   0x0040
    #define DOT11_CAPABILITY_CHANNEL_AGILITY        0x0080
    #define DOT11_CAPABILITY_SHORT_SLOT_TIME        0x0400
    #define DOT11_CAPABILITY_DSSSOFDM               0x2000

    typedef struct _DOT11_BSS_DESCRIPTION {
        ULONG uReserved;                        // Passed-in as 0 and must be ignored for now.
        DOT11_MAC_ADDRESS dot11BSSID;
        DOT11_BSS_TYPE dot11BSSType;
        USHORT usBeaconPeriod;
        ULONGLONG ullTimestamp;
        USHORT usCapabilityInformation;
        ULONG uBufferLength;
        UCHAR ucBuffer[1];              // Must be the last field.
    } DOT11_BSS_DESCRIPTION, * PDOT11_BSS_DESCRIPTION;
    typedef struct _DOT11_JOIN_REQUEST {
        ULONG uJoinFailureTimeout;
        DOT11_RATE_SET OperationalRateSet;
        ULONG uChCenterFrequency;
        DOT11_BSS_DESCRIPTION dot11BSSDescription;  // Must be the last field.
    } DOT11_JOIN_REQUEST, * PDOT11_JOIN_REQUEST;

#define OID_DOT11_START_REQUEST                     (OID_DOT11_NDIS_START + 14)
    typedef struct _DOT11_START_REQUEST {
        ULONG uStartFailureTimeout;
        DOT11_RATE_SET OperationalRateSet;
        ULONG uChCenterFrequency;
        DOT11_BSS_DESCRIPTION dot11BSSDescription;  // Must be the last field.
    } DOT11_START_REQUEST, * PDOT11_START_REQUEST;

#define OID_DOT11_UPDATE_IE                         (OID_DOT11_NDIS_START + 15)
typedef enum _DOT11_UPDATE_IE_OP {
    dot11_update_ie_op_create_replace = 1,
    dot11_update_ie_op_delete = 2,
} DOT11_UPDATE_IE_OP, * PDOT11_UPDATE_IE_OP;

typedef struct _DOT11_UPDATE_IE {
    DOT11_UPDATE_IE_OP dot11UpdateIEOp;
    ULONG uBufferLength;
    UCHAR ucBuffer[1];          // Must be the last field.
} DOT11_UPDATE_IE, * PDOT11_UPDATE_IE;

#define OID_DOT11_RESET_REQUEST                     (OID_DOT11_NDIS_START + 16)
    typedef enum _DOT11_RESET_TYPE {
        dot11_reset_type_phy = 1,
        dot11_reset_type_mac = 2,
        dot11_reset_type_phy_and_mac = 3
    } DOT11_RESET_TYPE, * PDOT11_RESET_TYPE;
    typedef struct _DOT11_RESET_REQUEST {
        DOT11_RESET_TYPE dot11ResetType;
        DOT11_MAC_ADDRESS dot11MacAddress;
        BOOLEAN bSetDefaultMIB;
    } DOT11_RESET_REQUEST, * PDOT11_RESET_REQUEST;

#define OID_DOT11_NIC_POWER_STATE                   (OID_DOT11_NDIS_START + 17)
    // BOOL

//
// OIDs for Optional Functions
//

#define OID_DOT11_OPTIONAL_CAPABILITY               (OID_DOT11_NDIS_START + 18)
    typedef struct _DOT11_OPTIONAL_CAPABILITY {
        ULONG uReserved;
        BOOLEAN bDot11PCF;
        BOOLEAN bDot11PCFMPDUTransferToPC;
        BOOLEAN bStrictlyOrderedServiceClass;
    } DOT11_OPTIONAL_CAPABILITY, * PDOT11_OPTIONAL_CAPABILITY;

#define OID_DOT11_CURRENT_OPTIONAL_CAPABILITY       (OID_DOT11_NDIS_START + 19)
    typedef struct _DOT11_CURRENT_OPTIONAL_CAPABILITY {
        ULONG uReserved;
        BOOLEAN bDot11CFPollable;
        BOOLEAN bDot11PCF;
        BOOLEAN bDot11PCFMPDUTransferToPC;
        BOOLEAN bStrictlyOrderedServiceClass;
    } DOT11_CURRENT_OPTIONAL_CAPABILITY, * PDOT11_CURRENT_OPTIONAL_CAPABILITY;

//
// 802.11 MIB OIDs
//

//
// OIDs for dot11StationConfigEntry
//

#define OID_DOT11_STATION_ID                        (OID_DOT11_NDIS_START + 20)
    // DOT11_MAC_ADDRESS

#define OID_DOT11_MEDIUM_OCCUPANCY_LIMIT            (OID_DOT11_NDIS_START + 21)
    // ULONG (in TUs)

#define OID_DOT11_CF_POLLABLE                       (OID_DOT11_NDIS_START + 22)
    // BOOL

#define OID_DOT11_CFP_PERIOD                        (OID_DOT11_NDIS_START + 23)
    // ULONG (in DTIM intervals)

#define OID_DOT11_CFP_MAX_DURATION                  (OID_DOT11_NDIS_START + 24)
    // ULONG (in TUs)

#define OID_DOT11_POWER_MGMT_MODE                   (OID_DOT11_NDIS_START + 25)
    typedef enum _DOT11_POWER_MODE {
        dot11_power_mode_unknown = 0,
        dot11_power_mode_active = 1,
        dot11_power_mode_powersave = 2
    } DOT11_POWER_MODE, * PDOT11_POWER_MODE;
    #define DOT11_POWER_SAVE_LEVEL_MAX_PSP      1
    // Maximum power save polling.
    #define DOT11_POWER_SAVE_LEVEL_FAST_PSP     2
    // Fast power save polling.
    typedef struct _DOT11_POWER_MGMT_MODE {
        DOT11_POWER_MODE dot11PowerMode;
        ULONG uPowerSaveLevel;
        USHORT usListenInterval;
        USHORT usAID;
        BOOLEAN bReceiveDTIMs;
    } DOT11_POWER_MGMT_MODE, * PDOT11_POWER_MGMT_MODE;

#define OID_DOT11_OPERATIONAL_RATE_SET              (OID_DOT11_NDIS_START + 26)
    // DOT11_RATE_SET

#define OID_DOT11_BEACON_PERIOD                     (OID_DOT11_NDIS_START + 27)
    // ULONG (in TUs)

#define OID_DOT11_DTIM_PERIOD                       (OID_DOT11_NDIS_START + 28)
    // ULONG (in beacon intervals)

//
// OIDs for Dot11PrivacyEntry
//

#define OID_DOT11_WEP_ICV_ERROR_COUNT               (OID_DOT11_NDIS_START + 29)
    // ULONG

//
// OIDs for dot11OperationEntry
//

#define OID_DOT11_MAC_ADDRESS                       (OID_DOT11_NDIS_START + 30)
    // DOT11_MAC_ADDRESS

#define OID_DOT11_RTS_THRESHOLD                     (OID_DOT11_NDIS_START + 31)
    // ULONG (in number of octets)

#define OID_DOT11_SHORT_RETRY_LIMIT                 (OID_DOT11_NDIS_START + 32)
    // ULONG

#define OID_DOT11_LONG_RETRY_LIMIT                  (OID_DOT11_NDIS_START + 33)
    // ULONG

#define OID_DOT11_FRAGMENTATION_THRESHOLD           (OID_DOT11_NDIS_START + 34)
    // ULONG (in number of octets)

#define OID_DOT11_MAX_TRANSMIT_MSDU_LIFETIME        (OID_DOT11_NDIS_START + 35)
    // ULONG (in TUs)

#define OID_DOT11_MAX_RECEIVE_LIFETIME              (OID_DOT11_NDIS_START + 36)
    // ULONG (in TUs)

//
// OIDs for dot11CountersEntry
//

#define OID_DOT11_COUNTERS_ENTRY                    (OID_DOT11_NDIS_START + 37)
    typedef struct _DOT11_COUNTERS_ENTRY {
        ULONG uTransmittedFragmentCount;
        ULONG uMulticastTransmittedFrameCount;
        ULONG uFailedCount;
        ULONG uRetryCount;
        ULONG uMultipleRetryCount;
        ULONG uFrameDuplicateCount;
        ULONG uRTSSuccessCount;
        ULONG uRTSFailureCount;
        ULONG uACKFailureCount;
        ULONG uReceivedFragmentCount;
        ULONG uMulticastReceivedFrameCount;
        ULONG uFCSErrorCount;
        ULONG uTransmittedFrameCount;
    } DOT11_COUNTERS_ENTRY, * PDOT11_COUNTERS_ENTRY;

//
// OIDs for dot11PhyOperationEntry
//

#define OID_DOT11_SUPPORTED_PHY_TYPES               (OID_DOT11_NDIS_START + 38)
    typedef struct _DOT11_SUPPORTED_PHY_TYPES {
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_PHY_TYPE dot11PHYType[1];
    } DOT11_SUPPORTED_PHY_TYPES, * PDOT11_SUPPORTED_PHY_TYPES;

#define OID_DOT11_CURRENT_REG_DOMAIN                (OID_DOT11_NDIS_START + 39)
    #define DOT11_REG_DOMAIN_OTHER                  0x00000000
    #define DOT11_REG_DOMAIN_FCC                    0x00000010
    #define DOT11_REG_DOMAIN_DOC                    0x00000020
    #define DOT11_REG_DOMAIN_ETSI                   0x00000030
    #define DOT11_REG_DOMAIN_SPAIN                  0x00000031
    #define DOT11_REG_DOMAIN_FRANCE                 0x00000032
    #define DOT11_REG_DOMAIN_MKK                    0x00000040
    // ULONG

#define OID_DOT11_TEMP_TYPE                         (OID_DOT11_NDIS_START + 40)
    typedef enum _DOT11_TEMP_TYPE {
        dot11_temp_type_unknown = 0,
        dot11_temp_type_1 = 1,
        dot11_temp_type_2 = 2
    } DOT11_TEMP_TYPE, * PDOT11_TEMP_TYPE;

//
// OIDs for dot11PhyAntennaEntry
//

#define OID_DOT11_CURRENT_TX_ANTENNA                (OID_DOT11_NDIS_START + 41)
    // ULONG

#define OID_DOT11_DIVERSITY_SUPPORT                 (OID_DOT11_NDIS_START + 42)
    typedef enum _DOT11_DIVERSITY_SUPPORT {
        dot11_diversity_support_unknown = 0,
        dot11_diversity_support_fixedlist = 1,
        dot11_diversity_support_notsupported = 2,
        dot11_diversity_support_dynamic = 3
    } DOT11_DIVERSITY_SUPPORT, * PDOT11_DIVERSITY_SUPPORT;

#define OID_DOT11_CURRENT_RX_ANTENNA                (OID_DOT11_NDIS_START + 43)
    // ULONG

//
// OIDs for dot11PhyTxPowerEntry
//

#define OID_DOT11_SUPPORTED_POWER_LEVELS            (OID_DOT11_NDIS_START + 44)
    typedef struct _DOT11_SUPPORTED_POWER_LEVELS {
        ULONG uNumOfSupportedPowerLevels;
        ULONG uTxPowerLevelValues[8];
    } DOT11_SUPPORTED_POWER_LEVELS, * PDOT11_SUPPORTED_POWER_LEVELS;

#define OID_DOT11_CURRENT_TX_POWER_LEVEL            (OID_DOT11_NDIS_START + 45)
    // ULONG

//
// OIDs for dot11PhyFHSSEntry
//

#define OID_DOT11_HOP_TIME                          (OID_DOT11_NDIS_START + 46)
    // ULONG (in microseconds)

#define OID_DOT11_CURRENT_CHANNEL_NUMBER            (OID_DOT11_NDIS_START + 47)
    // ULONG

#define OID_DOT11_MAX_DWELL_TIME                    (OID_DOT11_NDIS_START + 48)
    // ULONG (in TUs)

#define OID_DOT11_CURRENT_DWELL_TIME                (OID_DOT11_NDIS_START + 49)
    // ULONG (in TUs)

#define OID_DOT11_CURRENT_SET                       (OID_DOT11_NDIS_START + 50)
    // ULONG

#define OID_DOT11_CURRENT_PATTERN                   (OID_DOT11_NDIS_START + 51)
    // ULONG

#define OID_DOT11_CURRENT_INDEX                     (OID_DOT11_NDIS_START + 52)
    // ULONG

//
// OIDs for dot11PhyDSSSEntry
//

#define OID_DOT11_CURRENT_CHANNEL                   (OID_DOT11_NDIS_START + 53)
    // ULONG

#define OID_DOT11_CCA_MODE_SUPPORTED                (OID_DOT11_NDIS_START + 54)
    #define DOT11_CCA_MODE_ED_ONLY                  0x00000001
    #define DOT11_CCA_MODE_CS_ONLY                  0x00000002
    #define DOT11_CCA_MODE_ED_and_CS                0x00000004
    #define DOT11_CCA_MODE_CS_WITH_TIMER            0x00000008
    #define DOT11_CCA_MODE_HRCS_AND_ED              0x00000010

    // ULONG

#define OID_DOT11_CURRENT_CCA_MODE                  (OID_DOT11_NDIS_START + 55)
    // ULONG

#define OID_DOT11_ED_THRESHOLD                      (OID_DOT11_NDIS_START + 56)
    // LONG (in "dBm"s)

//
// OIDs for dot11PhyIREntry
//

#define OID_DOT11_CCA_WATCHDOG_TIMER_MAX            (OID_DOT11_NDIS_START + 57)
    // ULONG (in nanoseconds)

#define OID_DOT11_CCA_WATCHDOG_COUNT_MAX            (OID_DOT11_NDIS_START + 58)
    // ULONG

#define OID_DOT11_CCA_WATCHDOG_TIMER_MIN            (OID_DOT11_NDIS_START + 59)
    // ULONG (in nanoseconds)

#define OID_DOT11_CCA_WATCHDOG_COUNT_MIN            (OID_DOT11_NDIS_START + 60)
    // ULONG

//
// OIDs for dot11RegDomainsSupportEntry
//

#define OID_DOT11_REG_DOMAINS_SUPPORT_VALUE         (OID_DOT11_NDIS_START + 61)
    typedef struct _DOT11_REG_DOMAIN_VALUE {
        ULONG uRegDomainsSupportIndex;
        ULONG uRegDomainsSupportValue;
    } DOT11_REG_DOMAIN_VALUE, * PDOT11_REG_DOMAIN_VALUE;
    typedef struct _DOT11_REG_DOMAINS_SUPPORT_VALUE {
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_REG_DOMAIN_VALUE dot11RegDomainValue[1];
    } DOT11_REG_DOMAINS_SUPPORT_VALUE, * PDOT11_REG_DOMAINS_SUPPORT_VALUE;

//
// OIDs for dot11AntennaListEntry
//

#define OID_DOT11_SUPPORTED_TX_ANTENNA              (OID_DOT11_NDIS_START + 62)
    typedef struct _DOT11_SUPPORTED_ANTENNA {
        ULONG uAntennaListIndex;                    // Between 1 and 255.
        BOOLEAN bSupportedAntenna;
    } DOT11_SUPPORTED_ANTENNA, * PDOT11_SUPPORTED_ANTENNA;
    typedef struct _DOT11_SUPPORTED_ANTENNA_LIST {
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_SUPPORTED_ANTENNA dot11SupportedAntenna[1];
    } DOT11_SUPPORTED_ANTENNA_LIST, * PDOT11_SUPPORTED_ANTENNA_LIST;

#define OID_DOT11_SUPPORTED_RX_ANTENNA              (OID_DOT11_NDIS_START + 63)
    // DOT11_SUPPORTED_ANTENNA_LIST

#define OID_DOT11_DIVERSITY_SELECTION_RX            (OID_DOT11_NDIS_START + 64)
    typedef struct _DOT11_DIVERSITY_SELECTION_RX {
        ULONG uAntennaListIndex;                    // Between 1 and 255.
        BOOLEAN bDiversitySelectionRX;
    } DOT11_DIVERSITY_SELECTION_RX, * PDOT11_DIVERSITY_SELECTION_RX;
    typedef struct _DOT11_DIVERSITY_SELECTION_RX_LIST {
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_DIVERSITY_SELECTION_RX dot11DiversitySelectionRx[1];
    } DOT11_DIVERSITY_SELECTION_RX_LIST, * PDOT11_DIVERSITY_SELECTION_RX_LIST;

//
// OIDs for dot11SupportedDataRatesTxEntry and dot11SupportedDataRatesRxEntry
//

#define OID_DOT11_SUPPORTED_DATA_RATES_VALUE        (OID_DOT11_NDIS_START + 65)
    #define MAX_NUM_SUPPORTED_RATES                 8       // 8 data rates
    #define MAX_NUM_SUPPORTED_RATES_V2              255     // 255 data rates
    typedef struct _DOT11_SUPPORTED_DATA_RATES_VALUE {
        UCHAR ucSupportedTxDataRatesValue[MAX_NUM_SUPPORTED_RATES];
        UCHAR ucSupportedRxDataRatesValue[MAX_NUM_SUPPORTED_RATES];
    } DOT11_SUPPORTED_DATA_RATES_VALUE, * PDOT11_SUPPORTED_DATA_RATES_VALUE;

    typedef struct _DOT11_SUPPORTED_DATA_RATES_VALUE_V2 {
        UCHAR ucSupportedTxDataRatesValue[MAX_NUM_SUPPORTED_RATES_V2];
        UCHAR ucSupportedRxDataRatesValue[MAX_NUM_SUPPORTED_RATES_V2];
    } DOT11_SUPPORTED_DATA_RATES_VALUE_V2, * PDOT11_SUPPORTED_DATA_RATES_VALUE_V2;

    // keep the incorrect struct name to avoid build break
    typedef DOT11_SUPPORTED_DATA_RATES_VALUE_V2
        DOT11_SUPPORTED_DATA_RATES_VALUE_V1, * PDOT11_SUPPORTED_DATA_RATES_VALUE_V1;

//
// OIDs for dot11PhyOFDMEntry
//

#define OID_DOT11_CURRENT_FREQUENCY                 (OID_DOT11_NDIS_START + 66)
    // ULONG

#define OID_DOT11_TI_THRESHOLD                      (OID_DOT11_NDIS_START + 67)
    // LONG

#define OID_DOT11_FREQUENCY_BANDS_SUPPORTED         (OID_DOT11_NDIS_START + 68)
    #define DOT11_FREQUENCY_BANDS_LOWER    0x00000001
    #define DOT11_FREQUENCY_BANDS_MIDDLE   0x00000002
    #define DOT11_FREQUENCY_BANDS_UPPER    0x00000004
    // ULONG

//
// OIDs for dot11PhyHRDSSSEntry
//

#define OID_DOT11_SHORT_PREAMBLE_OPTION_IMPLEMENTED (OID_DOT11_NDIS_START + 69)
    // BOOL

#define OID_DOT11_PBCC_OPTION_IMPLEMENTED           (OID_DOT11_NDIS_START + 70)
    // BOOL

#define OID_DOT11_CHANNEL_AGILITY_PRESENT           (OID_DOT11_NDIS_START + 71)
    // BOOL

#define OID_DOT11_CHANNEL_AGILITY_ENABLED           (OID_DOT11_NDIS_START + 72)
    // BOOL

#define OID_DOT11_HR_CCA_MODE_SUPPORTED             (OID_DOT11_NDIS_START + 73)
    // HR-CCA mode bits
    #define DOT11_HR_CCA_MODE_ED_ONLY        0x00000001
    #define DOT11_HR_CCA_MODE_CS_ONLY        0x00000002
    #define DOT11_HR_CCA_MODE_CS_AND_ED      0x00000004
    #define DOT11_HR_CCA_MODE_CS_WITH_TIMER  0x00000008
    #define DOT11_HR_CCA_MODE_HRCS_AND_ED    0x00000010
    // ULONG


//
// OIDs for dot11StationConfigEntry (Cont)
//

#define OID_DOT11_MULTI_DOMAIN_CAPABILITY_IMPLEMENTED   (OID_DOT11_NDIS_START + 74)
    // BOOL

#define OID_DOT11_MULTI_DOMAIN_CAPABILITY_ENABLED       (OID_DOT11_NDIS_START + 75)
    // BOOL

#define OID_DOT11_COUNTRY_STRING                        (OID_DOT11_NDIS_START + 76)
    // UCHAR[3]

//
// OIDs for dot11MultiDomainCapabilityEntry
//

typedef struct _DOT11_MULTI_DOMAIN_CAPABILITY_ENTRY {
    ULONG uMultiDomainCapabilityIndex;
    ULONG uFirstChannelNumber;
    ULONG uNumberOfChannels;
    LONG lMaximumTransmitPowerLevel;
} DOT11_MULTI_DOMAIN_CAPABILITY_ENTRY, *PDOT11_MULTI_DOMAIN_CAPABILITY_ENTRY;
typedef struct _DOT11_MD_CAPABILITY_ENTRY_LIST {
    ULONG uNumOfEntries;
    ULONG uTotalNumOfEntries;
    DOT11_MULTI_DOMAIN_CAPABILITY_ENTRY dot11MDCapabilityEntry[1];
} DOT11_MD_CAPABILITY_ENTRY_LIST, *PDOT11_MD_CAPABILITY_ENTRY_LIST;


#define OID_DOT11_MULTI_DOMAIN_CAPABILITY           (OID_DOT11_NDIS_START + 77)
    // DOT11_MD_CAPABILITY_ENTRY_LIST

//
// OIDs for dot11PhyFHSSEntry
//

#define OID_DOT11_EHCC_PRIME_RADIX                  (OID_DOT11_NDIS_START + 78)
    // ULONG

#define OID_DOT11_EHCC_NUMBER_OF_CHANNELS_FAMILY_INDEX  (OID_DOT11_NDIS_START + 79)
    // ULONG

#define OID_DOT11_EHCC_CAPABILITY_IMPLEMENTED       (OID_DOT11_NDIS_START + 80)
    // BOOL

#define OID_DOT11_EHCC_CAPABILITY_ENABLED           (OID_DOT11_NDIS_START + 81)
    // BOOL

#define OID_DOT11_HOP_ALGORITHM_ADOPTED             (OID_DOT11_NDIS_START + 82)
    typedef enum _DOT11_HOP_ALGO_ADOPTED {
        dot11_hop_algo_current = 0,
        dot11_hop_algo_hop_index = 1,
        dot11_hop_algo_hcc = 2
    } DOT11_HOP_ALGO_ADOPTED, * PDOT11_HOP_ALGO_ADOPTED;

#define OID_DOT11_RANDOM_TABLE_FLAG                 (OID_DOT11_NDIS_START + 83)
    // BOOL

#define OID_DOT11_NUMBER_OF_HOPPING_SETS            (OID_DOT11_NDIS_START + 84)
    // ULONG

#define OID_DOT11_HOP_MODULUS                       (OID_DOT11_NDIS_START + 85)
    // ULONG

#define OID_DOT11_HOP_OFFSET                        (OID_DOT11_NDIS_START + 86)
    // ULONG


//
// OIDs for dot11HoppingPatternEntry
//
#define OID_DOT11_HOPPING_PATTERN                   (OID_DOT11_NDIS_START + 87)
typedef struct _DOT11_HOPPING_PATTERN_ENTRY {
    ULONG uHoppingPatternIndex;
    ULONG uRandomTableFieldNumber;
} DOT11_HOPPING_PATTERN_ENTRY, *PDOT11_HOPPING_PATTERN_ENTRY;
typedef struct _DOT11_HOPPING_PATTERN_ENTRY_LIST {
    ULONG uNumOfEntries;
    ULONG uTotalNumOfEntries;
    DOT11_HOPPING_PATTERN_ENTRY dot11HoppingPatternEntry[1];
} DOT11_HOPPING_PATTERN_ENTRY_LIST, *PDOT11_HOPPING_PATTERN_ENTRY_LIST;


#define OID_DOT11_RANDOM_TABLE_FIELD_NUMBER         (OID_DOT11_NDIS_START + 88)
    // ULONG

//
// WPA Extensions
//

#define OID_DOT11_WPA_TSC                           (OID_DOT11_NDIS_START + 89)
typedef struct _DOT11_WPA_TSC {
    ULONG uReserved;
    DOT11_OFFLOAD_TYPE dot11OffloadType;
    HANDLE hOffload;
    DOT11_IV48_COUNTER dot11IV48Counter;
} DOT11_WPA_TSC, * PDOT11_WPA_TSC;

//
// dot11.
//

#define OID_DOT11_RSSI_RANGE                        (OID_DOT11_NDIS_START + 90)
typedef struct _DOT11_RSSI_RANGE {
    DOT11_PHY_TYPE dot11PhyType;
    ULONG uRSSIMin; // Minimum caliberation value of RSSI in the NIC.
    ULONG uRSSIMax; // Maximum caliberation value of RSSI in the NIC.
} DOT11_RSSI_RANGE, * PDOT11_RSSI_RANGE;

#define OID_DOT11_RF_USAGE                          (OID_DOT11_NDIS_START + 91)
//ULONG

#define OID_DOT11_NIC_SPECIFIC_EXTENSION            (OID_DOT11_NDIS_START + 92)
typedef struct _DOT11_NIC_SPECIFIC_EXTENSION {
    ULONG uBufferLength;
    ULONG uTotalBufferLength;
    UCHAR ucBuffer[1];
} DOT11_NIC_SPECIFIC_EXTENSION, * PDOT11_NIC_SPECIFIC_EXTENSION;

//
// AP join request
//

#define OID_DOT11_AP_JOIN_REQUEST                   (OID_DOT11_NDIS_START + 93)
    typedef struct _DOT11_AP_JOIN_REQUEST {
        ULONG uJoinFailureTimeout;
        DOT11_RATE_SET OperationalRateSet;
        ULONG uChCenterFrequency;
        DOT11_BSS_DESCRIPTION dot11BSSDescription;  // Must be the last field.
    } DOT11_AP_JOIN_REQUEST, * PDOT11_AP_JOIN_REQUEST;

//
// dot11PhyERPEntry
//
#define OID_DOT11_ERP_PBCC_OPTION_IMPLEMENTED       (OID_DOT11_NDIS_START + 94)
    // BOOL

#define OID_DOT11_ERP_PBCC_OPTION_ENABLED           (OID_DOT11_NDIS_START + 95)
    // BOOL

#define OID_DOT11_DSSS_OFDM_OPTION_IMPLEMENTED      (OID_DOT11_NDIS_START + 96)
    // BOOL

#define OID_DOT11_DSSS_OFDM_OPTION_ENABLED          (OID_DOT11_NDIS_START + 97)
    // BOOL

#define OID_DOT11_SHORT_SLOT_TIME_OPTION_IMPLEMENTED    (OID_DOT11_NDIS_START + 98)
    // BOOL

#define OID_DOT11_SHORT_SLOT_TIME_OPTION_ENABLED    (OID_DOT11_NDIS_START + 99)
    // BOOL

#define OID_DOT11_MAX_MAC_ADDRESS_STATES            (OID_DOT11_NDIS_START + 100)
    // ULONG

#define OID_DOT11_RECV_SENSITIVITY_LIST             (OID_DOT11_NDIS_START + 101)
    // DOT11_RECV_SENSITIVITY_LIST

    typedef struct _DOT11_RECV_SENSITIVITY {
        UCHAR ucDataRate;
        LONG lRSSIMin;
        LONG lRSSIMax;
    } DOT11_RECV_SENSITIVITY, * PDOT11_RECV_SENSITIVITY;

    typedef struct _DOT11_RECV_SENSITIVITY_LIST {
        union {
            DOT11_PHY_TYPE dot11PhyType;
            ULONG uPhyId;
        };
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_RECV_SENSITIVITY dot11RecvSensitivity[1];
    } DOT11_RECV_SENSITIVITY_LIST, * PDOT11_RECV_SENSITIVITY_LIST;


//
// WME
//

#define OID_DOT11_WME_IMPLEMENTED                   (OID_DOT11_NDIS_START + 102)
    // BOOL

#define OID_DOT11_WME_ENABLED                       (OID_DOT11_NDIS_START + 103)
    // BOOL

#define OID_DOT11_WME_AC_PARAMETERS                 (OID_DOT11_NDIS_START + 104)
    typedef enum _DOT11_AC_PARAM {
        dot11_AC_param_BE = 0,      // Best Effort
        dot11_AC_param_BK = 1,      // Background
        dot11_AC_param_VI = 2,      // Video
        dot11_AC_param_VO = 3,      // Voice
        dot11_AC_param_max
    } DOT11_AC_PARAM, * PDOT11_AC_PARAM;
    typedef struct _DOT11_WME_AC_PARAMETERS {
	    UCHAR ucAccessCategoryIndex;
	    UCHAR ucAIFSN;
	    UCHAR ucECWmin;
	    UCHAR ucECWmax;
	    USHORT usTXOPLimit;
	} DOT11_WME_AC_PARAMETERS, * PDOT11_WME_AC_PARAMETERS;
	typedef struct _DOT11_WME_AC_PARAMTERS_LIST {
	    ULONG uNumOfEntries;
	    ULONG uTotalNumOfEntries;
	    DOT11_WME_AC_PARAMETERS dot11WMEACParameters[1];
    } DOT11_WME_AC_PARAMETERS_LIST, * PDOT11_WME_AC_PARAMETERS_LIST;

#define OID_DOT11_WME_UPDATE_IE                    (OID_DOT11_NDIS_START + 105)
    typedef struct _DOT11_WME_UPDATE_IE {
        ULONG uParamElemMinBeaconIntervals;
        ULONG uWMEInfoElemOffset;
        ULONG uWMEInfoElemLength;
        ULONG uWMEParamElemOffset;
        ULONG uWMEParamElemLength;
        UCHAR ucBuffer[1];          // Must be the last field.
    } DOT11_WME_UPDATE_IE, * PDOT11_WME_UPDATE_IE;

//
// QoS
//
#define OID_DOT11_QOS_TX_QUEUES_SUPPORTED          (OID_DOT11_NDIS_START + 106)
    // ULONG

#define OID_DOT11_QOS_TX_DURATION                  (OID_DOT11_NDIS_START + 107)
    typedef struct _DOT11_QOS_TX_DURATION {
        ULONG uNominalMSDUSize;
        ULONG uMinPHYRate;
        ULONG uDuration;
    } DOT11_QOS_TX_DURATION, * PDOT11_QOS_TX_DURATION;

#define OID_DOT11_QOS_TX_MEDIUM_TIME               (OID_DOT11_NDIS_START + 108)
    typedef struct _DOT11_QOS_TX_MEDIUM_TIME {
        DOT11_MAC_ADDRESS dot11PeerAddress;
        UCHAR ucQoSPriority;
        ULONG uMediumTimeAdmited;
    } DOT11_QOS_TX_MEDIUM_TIME, * PDOT11_QOS_TX_MEDIUM_TIME;

//
// NIC supported channel/center frequency list
//
#define OID_DOT11_SUPPORTED_OFDM_FREQUENCY_LIST    (OID_DOT11_NDIS_START + 109)
    typedef struct _DOT11_SUPPORTED_OFDM_FREQUENCY {
        ULONG uCenterFrequency;
    } DOT11_SUPPORTED_OFDM_FREQUENCY, * PDOT11_SUPPORTED_OFDM_FREQUENCY;
    typedef struct _DOT11_SUPPORTED_OFDM_FREQUENCY_LIST {
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_SUPPORTED_OFDM_FREQUENCY dot11SupportedOFDMFrequency[1];
    } DOT11_SUPPORTED_OFDM_FREQUENCY_LIST, * PDOT11_SUPPORTED_OFDM_FREQUENCY_LIST;

#define OID_DOT11_SUPPORTED_DSSS_CHANNEL_LIST      (OID_DOT11_NDIS_START + 110)
    typedef struct _DOT11_SUPPORTED_DSSS_CHANNEL {
        ULONG uChannel;
    } DOT11_SUPPORTED_DSSS_CHANNEL, * PDOT11_SUPPORTED_DSSS_CHANNEL;
    typedef struct _DOT11_SUPPORTED_DSSS_CHANNEL_LIST {
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_SUPPORTED_DSSS_CHANNEL dot11SupportedDSSSChannel[1];
    } DOT11_SUPPORTED_DSSS_CHANNEL_LIST, * PDOT11_SUPPORTED_DSSS_CHANNEL_LIST;


//
// Extensible STA
//

typedef struct DOT11_BYTE_ARRAY {
    NDIS_OBJECT_HEADER Header;
    ULONG uNumOfBytes;
    ULONG uTotalNumOfBytes;
    UCHAR ucBuffer[1];
} DOT11_BYTE_ARRAY, * PDOT11_BYTE_ARRAY;

#define OID_DOT11_AUTO_CONFIG_ENABLED               NWF_DEFINE_OID(120,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // ULONG
    #define DOT11_PHY_AUTO_CONFIG_ENABLED_FLAG          0x00000001U
    #define DOT11_MAC_AUTO_CONFIG_ENABLED_FLAG          0x00000002U

#define OID_DOT11_ENUM_BSS_LIST                     NWF_DEFINE_OID(121,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // DOT11_BYTE_ARRAY with DOT11_BSS_ENTRY
    #define DOT11_BSS_ENTRY_BYTE_ARRAY_REVISION_1   1

    // This structure is not supposed to be midl compliant because of
    // DOT11_BSS_ENTRY_PHY_SPECIFIC_INFO. The selection of union is
    // *indirectly* determined from uPhyId. MIDL will not work here.
    typedef union DOT11_BSS_ENTRY_PHY_SPECIFIC_INFO {
        ULONG uChCenterFrequency;
        struct {
            ULONG uHopPattern;
            ULONG uHopSet;
            ULONG uDwellTime;
        } FHSS;
    } DOT11_BSS_ENTRY_PHY_SPECIFIC_INFO, * PDOT11_BSS_ENTRY_PHY_SPECIFIC_INFO;

    typedef BWL_PRE_PACKED_STRUCT struct DOT11_BSS_ENTRY {
        ULONG uPhyId;
        DOT11_BSS_ENTRY_PHY_SPECIFIC_INFO PhySpecificInfo;
        DOT11_MAC_ADDRESS dot11BSSID;
        DOT11_BSS_TYPE dot11BSSType;
        LONG lRSSI;
        ULONG uLinkQuality;
        BOOLEAN bInRegDomain;
        USHORT usBeaconPeriod;
        ULONGLONG ullTimestamp;
        ULONGLONG ullHostTimestamp;
        USHORT usCapabilityInformation;
        ULONG uBufferLength;
        UCHAR ucBuffer[1];			// Must be the last field.
    } BWL_POST_PACKED_STRUCT DOT11_BSS_ENTRY, * PDOT11_BSS_ENTRY;

#define OID_DOT11_FLUSH_BSS_LIST                    NWF_DEFINE_OID(122,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // VOID

#define OID_DOT11_POWER_MGMT_REQUEST                NWF_DEFINE_OID(123,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // ULONG
    #define DOT11_POWER_SAVING_NO_POWER_SAVING  0
    #define DOT11_POWER_SAVING_FAST_PSP         8
    #define DOT11_POWER_SAVING_MAX_PSP          16
    #define DOT11_POWER_SAVING_MAXIMUM_LEVEL    24

#define OID_DOT11_DESIRED_SSID_LIST                 NWF_DEFINE_OID(124,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // A list of DOT11_SSID
    typedef struct DOT11_SSID_LIST {
        #define DOT11_SSID_LIST_REVISION_1  1
        NDIS_OBJECT_HEADER Header;
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_SSID SSIDs[1];
    } DOT11_SSID_LIST, * PDOT11_SSID_LIST;

#define OID_DOT11_EXCLUDED_MAC_ADDRESS_LIST         NWF_DEFINE_OID(125,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // A list of DOT11_MAC_ADDRESS
    typedef struct DOT11_MAC_ADDRESS_LIST {
        #define DOT11_MAC_ADDRESS_LIST_REVISION_1  1
        NDIS_OBJECT_HEADER Header;
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_MAC_ADDRESS MacAddrs[1];
    } DOT11_MAC_ADDRESS_LIST, * PDOT11_MAC_ADDRESS_LIST;

#define OID_DOT11_DESIRED_BSSID_LIST                NWF_DEFINE_OID(126,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)

#define OID_DOT11_DESIRED_BSS_TYPE                  NWF_DEFINE_OID(127,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // DOT11_BSS_TYPE

#define OID_DOT11_PMKID_LIST                        NWF_DEFINE_OID(128,NWF_OPERATIONAL_OID,NWF_OPTIONAL_OID)
    // A list of DOT11_PMKID_ENTRY
    typedef UCHAR DOT11_PMKID_VALUE[16];
    typedef struct DOT11_PMKID_ENTRY {
        DOT11_MAC_ADDRESS BSSID;
        DOT11_PMKID_VALUE PMKID;
        ULONG uFlags;
    } DOT11_PMKID_ENTRY, *PDOT11_PMKID_ENTRY;
    typedef struct DOT11_PMKID_LIST {
        #define DOT11_PMKID_LIST_REVISION_1  1
        NDIS_OBJECT_HEADER Header;
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_PMKID_ENTRY PMKIDs[1];
    } DOT11_PMKID_LIST, * PDOT11_PMKID_LIST;

#define OID_DOT11_CONNECT_REQUEST                   NWF_DEFINE_OID(129,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // no data type

#define OID_DOT11_EXCLUDE_UNENCRYPTED               NWF_DEFINE_OID(130,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // BOOLEAN

#define OID_DOT11_STATISTICS                        NWF_DEFINE_OID(131,NWF_STATISTICS_OID,NWF_MANDATORY_OID)
    // DOT11_STATISTICS structure
    #define DOT11_STATISTICS_UNKNOWN    (ULONGLONG)(-1LL)
    typedef struct DOT11_PHY_FRAME_STATISTICS {
        // TX counters (MSDU/MMPDU)
        ULONGLONG ullTransmittedFrameCount;
        ULONGLONG ullMulticastTransmittedFrameCount;
        ULONGLONG ullFailedCount;
        ULONGLONG ullRetryCount;
        ULONGLONG ullMultipleRetryCount;
        ULONGLONG ullMaxTXLifetimeExceededCount;

        // TX counters (MPDU)
        ULONGLONG ullTransmittedFragmentCount;
        ULONGLONG ullRTSSuccessCount;
        ULONGLONG ullRTSFailureCount;
        ULONGLONG ullACKFailureCount;

        // RX counters (MSDU/MMPDU)
        ULONGLONG ullReceivedFrameCount;
        ULONGLONG ullMulticastReceivedFrameCount;
        ULONGLONG ullPromiscuousReceivedFrameCount;
        ULONGLONG ullMaxRXLifetimeExceededCount;

        // RX counters (MPDU)
        ULONGLONG ullFrameDuplicateCount;
        ULONGLONG ullReceivedFragmentCount;
        ULONGLONG ullPromiscuousReceivedFragmentCount;
        ULONGLONG ullFCSErrorCount;
    } DOT11_PHY_FRAME_STATISTICS, * PDOT11_PHY_FRAME_STATISTICS;
    typedef struct DOT11_MAC_FRAME_STATISTICS {
        ULONGLONG ullTransmittedFrameCount;
        ULONGLONG ullReceivedFrameCount;
        ULONGLONG ullTransmittedFailureFrameCount;
        ULONGLONG ullReceivedFailureFrameCount;

        ULONGLONG ullWEPExcludedCount;
        ULONGLONG ullTKIPLocalMICFailures;
        ULONGLONG ullTKIPReplays;
        ULONGLONG ullTKIPICVErrorCount;
        ULONGLONG ullCCMPReplays;
        ULONGLONG ullCCMPDecryptErrors;
        ULONGLONG ullWEPUndecryptableCount;
        ULONGLONG ullWEPICVErrorCount;
        ULONGLONG ullDecryptSuccessCount;
        ULONGLONG ullDecryptFailureCount;
    } DOT11_MAC_FRAME_STATISTICS, * PDOT11_MAC_FRAME_STATISTICS;
    typedef BWL_PRE_PACKED_STRUCT struct DOT11_STATISTICS {
        #define DOT11_STATISTICS_REVISION_1  1
        NDIS_OBJECT_HEADER Header;
        ULONGLONG ullFourWayHandshakeFailures;
        ULONGLONG ullTKIPCounterMeasuresInvoked;
        ULONGLONG ullReserved;

        DOT11_MAC_FRAME_STATISTICS MacUcastCounters;
        DOT11_MAC_FRAME_STATISTICS MacMcastCounters;
        DOT11_PHY_FRAME_STATISTICS PhyCounters[1];
    } BWL_POST_PACKED_STRUCT DOT11_STATISTICS, * PDOT11_STATISTICS;

#define OID_DOT11_PRIVACY_EXEMPTION_LIST            NWF_DEFINE_OID(132,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // A list of DOT11_PRIVACY_EXEMPTION
    typedef struct DOT11_PRIVACY_EXEMPTION {
        USHORT usEtherType;

        #define DOT11_EXEMPT_NO_EXEMPTION       0   // used only in DOT11_EXTSTA_SEND_CONTEXT
        #define DOT11_EXEMPT_ALWAYS             1
        #define DOT11_EXEMPT_ON_KEY_MAPPING_KEY_UNAVAILABLE 2

        USHORT usExemptionActionType;

        #define DOT11_EXEMPT_UNICAST		1
        #define DOT11_EXEMPT_MULTICAST	        2
        #define DOT11_EXEMPT_BOTH		3
        USHORT usExemptionPacketType;
    } DOT11_PRIVACY_EXEMPTION, *PDOT11_PRIVACY_EXEMPTION;
    typedef struct DOT11_PRIVACY_EXEMPTION_LIST {
        #define DOT11_PRIVACY_EXEMPTION_LIST_REVISION_1  1
        NDIS_OBJECT_HEADER Header;
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_PRIVACY_EXEMPTION PrivacyExemptionEntries[1];
    } DOT11_PRIVACY_EXEMPTION_LIST, * PDOT11_PRIVACY_EXEMPTION_LIST;

#define OID_DOT11_ENABLED_AUTHENTICATION_ALGORITHM  NWF_DEFINE_OID(133,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    typedef struct DOT11_AUTH_ALGORITHM_LIST {
        #define DOT11_AUTH_ALGORITHM_LIST_REVISION_1  1
        NDIS_OBJECT_HEADER Header;
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_AUTH_ALGORITHM AlgorithmIds[1];
    } DOT11_AUTH_ALGORITHM_LIST, * PDOT11_AUTH_ALGORITHM_LIST;

#define OID_DOT11_SUPPORTED_UNICAST_ALGORITHM_PAIR  NWF_DEFINE_OID(134,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    typedef struct DOT11_AUTH_CIPHER_PAIR_LIST {
        #define DOT11_AUTH_CIPHER_PAIR_LIST_REVISION_1  1
        NDIS_OBJECT_HEADER Header;
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_AUTH_CIPHER_PAIR AuthCipherPairs[1];
    } DOT11_AUTH_CIPHER_PAIR_LIST, * PDOT11_AUTH_CIPHER_PAIR_LIST;

#define OID_DOT11_ENABLED_UNICAST_CIPHER_ALGORITHM  NWF_DEFINE_OID(135,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // DOT11_CIPHER_ALGO_LIST
    typedef struct DOT11_CIPHER_ALGORITHM_LIST {
        #define DOT11_CIPHER_ALGORITHM_LIST_REVISION_1  1
        NDIS_OBJECT_HEADER Header;
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_CIPHER_ALGORITHM AlgorithmIds[1];
    } DOT11_CIPHER_ALGORITHM_LIST, * PDOT11_CIPHER_ALGORITHM_LIST;

#define OID_DOT11_SUPPORTED_MULTICAST_ALGORITHM_PAIR    NWF_DEFINE_OID(136,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)

#define OID_DOT11_ENABLED_MULTICAST_CIPHER_ALGORITHM    NWF_DEFINE_OID(137,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // DOT11_CIPHER_ALGORITHM_LIST

#define OID_DOT11_CIPHER_DEFAULT_KEY_ID             NWF_DEFINE_OID(138,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // ULONG

#define OID_DOT11_CIPHER_DEFAULT_KEY                NWF_DEFINE_OID(139,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    typedef struct DOT11_CIPHER_DEFAULT_KEY_VALUE {
        #define DOT11_CIPHER_DEFAULT_KEY_VALUE_REVISION_1  1
        NDIS_OBJECT_HEADER Header;
        ULONG uKeyIndex;
        DOT11_CIPHER_ALGORITHM AlgorithmId;
        DOT11_MAC_ADDRESS MacAddr;
        BOOLEAN bDelete;
        BOOLEAN bStatic;
        USHORT usKeyLength;
        UCHAR ucKey[1];			// Must be the last field
    } DOT11_CIPHER_DEFAULT_KEY_VALUE, * PDOT11_CIPHER_DEFAULT_KEY_VALUE;
    typedef struct DOT11_KEY_ALGO_TKIP_MIC {
        UCHAR ucIV48Counter[6];
        ULONG ulTKIPKeyLength;
        ULONG ulMICKeyLength;
        UCHAR ucTKIPMICKeys[1];                     // Must be the last field.
    } DOT11_KEY_ALGO_TKIP_MIC, * PDOT11_KEY_ALGO_TKIP_MIC;
    typedef struct DOT11_KEY_ALGO_CCMP {
        UCHAR ucIV48Counter[6];
        ULONG ulCCMPKeyLength;
        UCHAR ucCCMPKey[1];
    } DOT11_KEY_ALGO_CCMP, * PDOT11_KEY_ALGO_CCMP;

#define OID_DOT11_CIPHER_KEY_MAPPING_KEY            NWF_DEFINE_OID(140,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // DOT11_BYTE_ARRAY
    typedef enum DOT11_DIRECTION {
        DOT11_DIR_INBOUND = 1,
        DOT11_DIR_OUTBOUND,
        DOT11_DIR_BOTH
    } DOT11_DIRECTION, * PDOT11_DIRECTION;
    #define DOT11_CIPHER_KEY_MAPPING_KEY_VALUE_BYTE_ARRAY_REVISION_1  1
    typedef struct DOT11_CIPHER_KEY_MAPPING_KEY_VALUE {
        DOT11_MAC_ADDRESS PeerMacAddr;
        DOT11_CIPHER_ALGORITHM AlgorithmId;
        DOT11_DIRECTION Direction;
        BOOLEAN bDelete;
        BOOLEAN bStatic;
        USHORT usKeyLength;
        UCHAR ucKey[1];			// Must be the last field
    } DOT11_CIPHER_KEY_MAPPING_KEY_VALUE, * PDOT11_CIPHER_KEY_MAPPING_KEY_VALUE;
#define OID_DOT11_ENUM_ASSOCIATION_INFO             NWF_DEFINE_OID(141,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // a list of DOT11_ASSOCIATION_INFO
    typedef enum _DOT11_ASSOCIATION_STATE {
        dot11_assoc_state_zero = 0,
        dot11_assoc_state_unauth_unassoc = 1,
        dot11_assoc_state_auth_unassoc = 2,
        dot11_assoc_state_auth_assoc = 3
    } DOT11_ASSOCIATION_STATE, * PDOT11_ASSOCIATION_STATE;
    typedef BWL_PRE_PACKED_STRUCT struct _DOT11_ASSOCIATION_INFO_EX {
        DOT11_MAC_ADDRESS PeerMacAddress;
        DOT11_MAC_ADDRESS BSSID;
        USHORT usCapabilityInformation;
        USHORT usListenInterval;
        UCHAR ucPeerSupportedRates[MAX_NUM_SUPPORTED_RATES_V2];
        USHORT usAssociationID;
        DOT11_ASSOCIATION_STATE dot11AssociationState;
        DOT11_POWER_MODE dot11PowerMode;
        LARGE_INTEGER liAssociationUpTime;
        ULONGLONG ullNumOfTxPacketSuccesses;
        ULONGLONG ullNumOfTxPacketFailures;
        ULONGLONG ullNumOfRxPacketSuccesses;
        ULONGLONG ullNumOfRxPacketFailures;
    } BWL_POST_PACKED_STRUCT DOT11_ASSOCIATION_INFO_EX, * PDOT11_ASSOCIATION_INFO_EX;
    typedef struct BWL_PRE_PACKED_STRUCT DOT11_ASSOCIATION_INFO_LIST {
        #define DOT11_ASSOCIATION_INFO_LIST_REVISION_1  1
        NDIS_OBJECT_HEADER Header;
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_ASSOCIATION_INFO_EX dot11AssocInfo[1];
    } BWL_POST_PACKED_STRUCT DOT11_ASSOCIATION_INFO_LIST, * PDOT11_ASSOCIATION_INFO_LIST;


#define OID_DOT11_DISCONNECT_REQUEST                NWF_DEFINE_OID(142,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)

#define OID_DOT11_UNICAST_USE_GROUP_ENABLED         NWF_DEFINE_OID(143,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // BOOLEAN

#define OID_DOT11_HARDWARE_PHY_STATE                NWF_DEFINE_OID(144,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // BOOLEAN

#define OID_DOT11_DESIRED_PHY_LIST                  NWF_DEFINE_OID(145,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // DOT11_PHY_ID_LIST
    typedef struct DOT11_PHY_ID_LIST {
        #define DOT11_PHY_ID_LIST_REVISION_1  1
        NDIS_OBJECT_HEADER Header;
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        ULONG dot11PhyId[1];
    } DOT11_PHY_ID_LIST, * PDOT11_PHY_ID_LIST;
    #define DOT11_PHY_ID_ANY        (0xffffffffU)

#define OID_DOT11_CURRENT_PHY_ID                    NWF_DEFINE_OID(146,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // ULONG

#define OID_DOT11_MEDIA_STREAMING_ENABLED           NWF_DEFINE_OID(147,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // BOOLEAN

#define OID_DOT11_UNREACHABLE_DETECTION_THRESHOLD   NWF_DEFINE_OID(148,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // ULONG

#define OID_DOT11_ACTIVE_PHY_LIST                   NWF_DEFINE_OID(149,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // DOT11_PHY_ID_LIST

#define OID_DOT11_EXTSTA_CAPABILITY                 NWF_DEFINE_OID(150,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // DOT11_EXTSTA_CAPABILITY
    typedef struct DOT11_EXTSTA_CAPABILITY {
        #define DOT11_EXTSTA_CAPABILITY_REVISION_1  1
        NDIS_OBJECT_HEADER Header;
        ULONG uScanSSIDListSize;
        ULONG uDesiredBSSIDListSize;
        ULONG uDesiredSSIDListSize;
        ULONG uExcludedMacAddressListSize;
        ULONG uPrivacyExemptionListSize;
        ULONG uKeyMappingTableSize;
        ULONG uDefaultKeyTableSize;
        ULONG uWEPKeyValueMaxLength;
        ULONG uPMKIDCacheSize;
        ULONG uMaxNumPerSTADefaultKeyTables;
    } DOT11_EXTSTA_CAPABILITY, * PDOT11_EXTSTA_CAPABILITY;

#define OID_DOT11_DATA_RATE_MAPPING_TABLE           NWF_DEFINE_OID(151,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // DOT11_DATA_RATE_MAPPING_ENTRY
    typedef struct DOT11_DATA_RATE_MAPPING_ENTRY {
        UCHAR ucDataRateIndex;
        UCHAR ucDataRateFlag;
        USHORT usDataRateValue;
    } DOT11_DATA_RATE_MAPPING_ENTRY, * PDOT11_DATA_RATE_MAPPING_ENTRY;
    typedef struct _DOT11_DATA_RATE_MAPPING_TABLE {
        #define DOT11_DATA_RATE_MAPPING_TABLE_REVISION_1  1
        NDIS_OBJECT_HEADER Header;
        ULONG uDataRateMappingLength;
	DOT11_DATA_RATE_MAPPING_ENTRY DataRateMappingEntries[DOT11_RATE_SET_MAX_LENGTH];
    } DOT11_DATA_RATE_MAPPING_TABLE, * PDOT11_DATA_RATE_MAPPING_TABLE;
    #define DOT11_DATA_RATE_NON_STANDARD        0x01U
    #define DOT11_DATA_RATE_INDEX_MASK          0x7fU

#define OID_DOT11_SUPPORTED_COUNTRY_OR_REGION_STRING    NWF_DEFINE_OID(152,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    typedef struct DOT11_COUNTRY_OR_REGION_STRING_LIST {
        #define DOT11_COUNTRY_OR_REGION_STRING_LIST_REVISION_1  1
        NDIS_OBJECT_HEADER Header;
        ULONG uNumOfEntries;
        ULONG uTotalNumOfEntries;
        DOT11_COUNTRY_OR_REGION_STRING CountryOrRegionStrings[1];
    } DOT11_COUNTRY_OR_REGION_STRING_LIST, * PDOT11_COUNTRY_OR_REGION_STRING_LIST;

#define OID_DOT11_DESIRED_COUNTRY_OR_REGION_STRING      NWF_DEFINE_OID(153,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // DOT11_COUNTRY_OR_REGION_STRING

#define OID_DOT11_PORT_STATE_NOTIFICATION           NWF_DEFINE_OID(154,NWF_OPERATIONAL_OID,NWF_OPTIONAL_OID)
    // DOT11_PORT_STATE_NOTIFICATION
    typedef struct DOT11_PORT_STATE_NOTIFICATION {
        #define DOT11_PORT_STATE_NOTIFICATION_REVISION_1  1
        NDIS_OBJECT_HEADER Header;
        DOT11_MAC_ADDRESS PeerMac;
        BOOLEAN bOpen;
    } DOT11_PORT_STATE_NOTIFICATION, * PDOT11_PORT_STATE_NOTIFICATION;

#define OID_DOT11_IBSS_PARAMS                       NWF_DEFINE_OID(155,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // DOT11_IBSS_PARAMS
    typedef struct DOT11_IBSS_PARAMS {
        #define DOT11_IBSS_PARAMS_REVISION_1  1
        NDIS_OBJECT_HEADER Header;
        BOOLEAN bJoinOnly;
        ULONG uIEsOffset;
        ULONG uIEsLength;
    } DOT11_IBSS_PARAMS, * PDOT11_IBSS_PARAMS;

#define OID_DOT11_QOS_PARAMS                        NWF_DEFINE_OID(156,NWF_OPERATIONAL_OID,NWF_OPTIONAL_OID)
    typedef struct DOT11_QOS_PARAMS {
        #define DOT11_QOS_PARAMS_REVISION_1     1
        NDIS_OBJECT_HEADER Header;

        #define DOT11_QOS_PROTOCOL_FLAG_WMM     (0x01U)         // WMM QoS protocol
        #define DOT11_QOS_PROTOCOL_FLAG_11E     (0x02U)         // 802.11e QoS protocol

        // Flags of the enabled QoS protocols.
        // It is either 0 or combination of DOT11_QOS_PROTOCOL_FLAG_WMM
        // and/or DOT11_QOS_PROTOCOL_FLAG_11E
        UCHAR ucEnabledQoSProtocolFlags;
    } DOT11_QOS_PARAMS, * PDOT11_QOS_PARAMS;

#define OID_DOT11_SAFE_MODE_ENABLED                 NWF_DEFINE_OID(157,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // BOOLEAN

#define OID_DOT11_HIDDEN_NETWORK_ENABLED            NWF_DEFINE_OID(158,NWF_OPERATIONAL_OID,NWF_MANDATORY_OID)
    // BOOLEAN

    typedef struct DOT11_PHY_ATTRIBUTES DOT11_PHY_ATTRIBUTES, * PDOT11_PHY_ATTRIBUTES;

    typedef struct DOT11_HRDSSS_PHY_ATTRIBUTES {
        BOOLEAN bShortPreambleOptionImplemented;
        BOOLEAN bPBCCOptionImplemented;
        BOOLEAN bChannelAgilityPresent;
        ULONG uHRCCAModeSupported;
    } DOT11_HRDSSS_PHY_ATTRIBUTES, * PDOT11_HRDSSS_PHY_ATTRIBUTES;

    typedef struct DOT11_OFDM_PHY_ATTRIBUTES {
        ULONG uFrequencyBandsSupported;
    } DOT11_OFDM_PHY_ATTRIBUTES, * PDOT11_OFDM_PHY_ATTRIBUTES;

    typedef struct DOT11_ERP_PHY_ATTRIBUTES {
        DOT11_HRDSSS_PHY_ATTRIBUTES HRDSSSAttributes;
        BOOLEAN bERPPBCCOptionImplemented;
        BOOLEAN bDSSSOFDMOptionImplemented;
        BOOLEAN bShortSlotTimeOptionImplemented;
    } DOT11_ERP_PHY_ATTRIBUTES, * PDOT11_ERP_PHY_ATTRIBUTES;

    struct DOT11_PHY_ATTRIBUTES {
        #define DOT11_PHY_ATTRIBUTES_REVISION_1  1
        NDIS_OBJECT_HEADER Header;

        DOT11_PHY_TYPE PhyType;
        BOOLEAN bHardwarePhyState;
        BOOLEAN bSoftwarePhyState;

        BOOLEAN bCFPollable;
        ULONG uMPDUMaxLength;
        DOT11_TEMP_TYPE TempType;
        DOT11_DIVERSITY_SUPPORT DiversitySupport;
        #ifdef __midl
        [switch_is(PhyType)]
        #endif
        union {
            #ifdef __midl
            [case(dot11_phy_type_hrdsss)]
            #endif
            DOT11_HRDSSS_PHY_ATTRIBUTES HRDSSSAttributes;

            #ifdef __midl
            [case(dot11_phy_type_ofdm)]
            #endif
            DOT11_OFDM_PHY_ATTRIBUTES OFDMAttributes;

            #ifdef __midl
            [case(dot11_phy_type_erp)]
            #endif
            DOT11_ERP_PHY_ATTRIBUTES ERPAttributes;
        #ifdef __cplusplus
        } PhySpecificAttributes;
        #else
        };
        #endif
        ULONG uNumberSupportedPowerLevels;
        ULONG TxPowerLevels[8];

        ULONG uNumDataRateMappingEntries;
        DOT11_DATA_RATE_MAPPING_ENTRY DataRateMappingEntries[DOT11_RATE_SET_MAX_LENGTH];

        DOT11_SUPPORTED_DATA_RATES_VALUE_V2 SupportedDataRatesValue;
    };

typedef enum DOT11_DS_INFO {
    DOT11_DS_CHANGED,
    DOT11_DS_UNCHANGED,
    DOT11_DS_UNKNOWN
} DOT11_DS_INFO, * PDOT11_DS_INFO;

/////////////////////////////////////////////
// Definitions of association status codes
//
typedef ULONG DOT11_ASSOC_STATUS;

// The association is successful
#define DOT11_ASSOC_STATUS_SUCCESS                          0

// Generic association failure
#define DOT11_ASSOC_STATUS_FAILURE                          0x00000001U

// The association fails because the peer is not responding.
// Scenarios:
//    1. the peer doesn't respond to 802.11 authentication frames or
//       802.11 association request frames or probe request frames.
//    2. the NIC hasn't received beacon from the peer for substantial
//       amount of time. The timeout value here is NIC specific.
//    3. any other cases in which NIC determines that the peer is not
//       responsive.
#define DOT11_ASSOC_STATUS_UNREACHABLE                      0x00000002U

// The association fails because the radio is turned off
#define DOT11_ASSOC_STATUS_RADIO_OFF                        0x00000003U

// The association fails because the PHY is disabled. Here the PHY
// entity becomes unavailable to the OS. But the radio itself is not
// necessarily turned off.
#define DOT11_ASSOC_STATUS_PHY_DISABLED                     0x00000004U

// The association is cancelled (for example, the NIC is reset)
#define DOT11_ASSOC_STATUS_CANCELLED                        0x00000005U

// The connection fails because all the candidate AP has been tried
// and none of the attempts succeeds.
#define DOT11_ASSOC_STATUS_CANDIDATE_LIST_EXHAUSTED         0x00000006U

// The current association is disassociated as requested by the OS
// (through either a OID_DOT11_RESET_REQUEST or OID_DOT11_DISCONNECT request)
#define DOT11_ASSOC_STATUS_DISASSOCIATED_BY_OS              0x00000007U

// The current association is disassociated because the NIC roams
// to new AP.
// This error code is used for indicating the implicit dissassociation
// done by the nwifi.sys. Miniport driver usually doesn't generate
// this error code (since the disassociation is automatically done
// by the nwifi.sys).
#define DOT11_ASSOC_STATUS_DISASSOCIATED_BY_ROAMING         0x00000008U

// The current association is disassocated because the NIC is reset
#define DOT11_ASSOC_STATUS_DISASSOCIATED_BY_RESET           0x00000009U

// The current association is disassocated because the NIC is reset
#define DOT11_ASSOC_STATUS_SYSTEM_ERROR                     0x0000000aU

// Roaming reason: find a better AP
#define DOT11_ASSOC_STATUS_ROAMING_BETTER_AP_FOUND          0x0000000bU

// Roaming reason: the association to the current BSS is lost
#define DOT11_ASSOC_STATUS_ROAMING_ASSOCIATION_LOST         0x0000000cU

// Roaming reason: adhoc roaming (network Coalescing)
#define DOT11_ASSOC_STATUS_ROAMING_ADHOC                    0x0000000dU

// The new association fails or the current association is disassocated
// because the NIC receives an 802.11 de-authentication frame from the
// peer. The lowest 16-bits are the reason code (2-byte) copied from
// the 802.11 DeAuthentication frame.
#define DOT11_ASSOC_STATUS_PEER_DEAUTHENTICATED             0x00010000U
#define DOT11_ASSOC_STATUS_PEER_DEAUTHENTICATED_START       DOT11_ASSOC_STATUS_PEER_DEAUTHENTICATED
#define DOT11_ASSOC_STATUS_PEER_DEAUTHENTICATED_END         0x0001ffffU

// The new association fails or the current association is disassocated
// because the NIC receives an 802.11 disassociation frame from the
// peer. The lowest 16-bits are the reason code (2-byte) copied from
// the 802.11 DisAssociation frame.
#define DOT11_ASSOC_STATUS_PEER_DISASSOCIATED               0x00020000U
#define DOT11_ASSOC_STATUS_PEER_DISASSOCIATED_START         DOT11_ASSOC_STATUS_PEER_DISASSOCIATED
#define DOT11_ASSOC_STATUS_PEER_DISASSOCIATED_END           0x0002ffffU

#define DOT11_ASSOC_STATUS_ASSOCIATION_RESPONSE             0x00030000U
#define DOT11_ASSOC_STATUS_ASSOCIATION_RESPONSE_START       DOT11_ASSOC_STATUS_ASSOCIATION_RESPONSE
#define DOT11_ASSOC_STATUS_ASSOCIATION_RESPONSE_END         0x0003ffffU

// The mask for extracting 802.11 deauthentication and disassociation
// reason code.
#define DOT11_ASSOC_STATUS_REASON_CODE_MASK                 0xffffU

// Define the range of IHV specific association status codes
#define DOT11_ASSOC_STATUS_IHV_START                        0x80000000U
#define DOT11_ASSOC_STATUS_IHV_END                          0xffffffffU

typedef struct DOT11_ASSOCIATION_COMPLETION_PARAMETERS {
    #define DOT11_ASSOCIATION_COMPLETION_PARAMETERS_REVISION_1  1
    NDIS_OBJECT_HEADER Header;
    DOT11_MAC_ADDRESS MacAddr;

    DOT11_ASSOC_STATUS uStatus;

    BOOLEAN bReAssocReq;
    BOOLEAN bReAssocResp;
    ULONG uAssocReqOffset, uAssocReqSize;
    ULONG uAssocRespOffset, uAssocRespSize;
    ULONG uBeaconOffset, uBeaconSize;
    ULONG uIHVDataOffset, uIHVDataSize;

    // The following fields are applicable for successful association.
    // For association failure, they must be zero-ed out.
    DOT11_AUTH_ALGORITHM AuthAlgo;
    DOT11_CIPHER_ALGORITHM UnicastCipher;
    DOT11_CIPHER_ALGORITHM MulticastCipher;
    ULONG uActivePhyListOffset, uActivePhyListSize;
    BOOLEAN bFourAddressSupported;
    BOOLEAN bPortAuthorized;

    // The QoS protocol which is used in this association.
    // It is zero or combination of DOT11_QOS_PROTOCOL_FLAG_WMM and/or DOT11_QOS_PROTOCOL_FLAG_11E
    UCHAR ucActiveQoSProtocol;

    DOT11_DS_INFO DSInfo;
    ULONG uEncapTableOffset, uEncapTableSize;
} DOT11_ASSOCIATION_COMPLETION_PARAMETERS, * PDOT11_ASSOCIATION_COMPLETION_PARAMETERS;

typedef struct DOT11_CONNECTION_START_PARAMETERS {
    #define DOT11_CONNECTION_START_PARAMETERS_REVISION_1  1
    NDIS_OBJECT_HEADER Header;
    DOT11_BSS_TYPE BSSType;

    DOT11_MAC_ADDRESS AdhocBSSID;   // applicable to adhoc mode only
    DOT11_SSID AdhocSSID;   // applicable to adhoc mode only
} DOT11_CONNECTION_START_PARAMETERS, * PDOT11_CONNECTION_START_PARAMETERS;

// For uStatus in DOT11_CONNECTION_COMPLETION_PARAMETERS and DOT11_ROAMING_COMPLETION_PARAMETERS
#define DOT11_CONNECTION_STATUS_SUCCESS                     DOT11_ASSOC_STATUS_SUCCESS
#define DOT11_CONNECTION_STATUS_FAILURE                     DOT11_ASSOC_STATUS_FAILURE
#define DOT11_CONNECTION_STATUS_CANDIDATE_LIST_EXHAUSTED    DOT11_ASSOC_STATUS_CANDIDATE_LIST_EXHAUSTED
#define DOT11_CONNECTION_STATUS_PHY_POWER_DOWN              DOT11_ASSOC_STATUS_RADIO_OFF
#define DOT11_CONNECTION_STATUS_CANCELLED                   DOT11_ASSOC_STATUS_CANCELLED
#define DOT11_CONNECTION_STATUS_IHV_START                   DOT11_ASSOC_STATUS_IHV_START
#define DOT11_CONNECTION_STATUS_IHV_END                     DOT11_ASSOC_STATUS_IHV_END
typedef struct DOT11_CONNECTION_COMPLETION_PARAMETERS {
    #define DOT11_CONNECTION_COMPLETION_PARAMETERS_REVISION_1  1
    NDIS_OBJECT_HEADER Header;
    DOT11_ASSOC_STATUS uStatus;  // DOT11_CONNECTION_STATUS_XXXX
} DOT11_CONNECTION_COMPLETION_PARAMETERS, * PDOT11_CONNECTION_COMPLETION_PARAMETERS;

#define DOT11_ROAMING_REASON_BETTER_AP_FOUND    DOT11_ASSOC_STATUS_ROAMING_BETTER_AP_FOUND
#define DOT11_ROAMING_REASON_ASSOCIATION_LOST   DOT11_ASSOC_STATUS_ROAMING_ASSOCIATION_LOST
#define DOT11_ROAMING_REASON_ADHOC              DOT11_ASSOC_STATUS_ROAMING_ADHOC
#define DOT11_ROAMING_REASON_IHV_START          DOT11_ASSOC_STATUS_IHV_START
#define DOT11_ROAMING_REASON_IHV_END            DOT11_ASSOC_STATUS_IHV_END
typedef struct DOT11_ROAMING_START_PARAMETERS {
    #define DOT11_ROAMING_START_PARAMETERS_REVISION_1  1
    NDIS_OBJECT_HEADER Header;
    DOT11_MAC_ADDRESS AdhocBSSID;   // applicable to adhoc mode only
    DOT11_SSID AdhocSSID;   // applicable to adhoc mode only
    DOT11_ASSOC_STATUS uRoamingReason;
} DOT11_ROAMING_START_PARAMETERS, * PDOT11_ROAMING_START_PARAMETERS;

typedef struct DOT11_ROAMING_COMPLETION_PARAMETERS {
    #define DOT11_ROAMING_COMPLETION_PARAMETERS_REVISION_1  1
    NDIS_OBJECT_HEADER Header;
    DOT11_ASSOC_STATUS uStatus;  // DOT11_CONNECTION_STATUS_XXXX
} DOT11_ROAMING_COMPLETION_PARAMETERS, * PDOT11_ROAMING_COMPLETION_PARAMETERS;

// Disassociation Reason Code
#define DOT11_DISASSOC_REASON_OS                    DOT11_ASSOC_STATUS_DISASSOCIATED_BY_OS
#define DOT11_DISASSOC_REASON_PEER_UNREACHABLE      DOT11_ASSOC_STATUS_UNREACHABLE
#define DOT11_DISASSOC_REASON_RADIO_OFF             DOT11_ASSOC_STATUS_RADIO_OFF
#define DOT11_DISASSOC_REASON_PHY_DISABLED          DOT11_ASSOC_STATUS_PHY_DISABLED
#define DOT11_DISASSOC_REASON_IHV_START             DOT11_ASSOC_STATUS_IHV_START
#define DOT11_DISASSOC_REASON_IHV_END               DOT11_ASSOC_STATUS_IHV_END
typedef struct DOT11_DISASSOCIATION_PARAMETERS {
    #define DOT11_DISASSOCIATION_PARAMETERS_REVISION_1  1
    NDIS_OBJECT_HEADER Header;
    DOT11_MAC_ADDRESS MacAddr;
    DOT11_ASSOC_STATUS uReason;
    ULONG uIHVDataOffset, uIHVDataSize;
} DOT11_DISASSOCIATION_PARAMETERS, * PDOT11_DISASSOCIATION_PARAMETERS;

typedef struct DOT11_TKIPMIC_FAILURE_PARAMETERS {
    #define DOT11_TKIPMIC_FAILURE_PARAMETERS_REVISION_1  1
    NDIS_OBJECT_HEADER Header;
    BOOLEAN bDefaultKeyFailure;
    ULONG uKeyIndex;
    DOT11_MAC_ADDRESS PeerMac;
} DOT11_TKIPMIC_FAILURE_PARAMETERS, * PDOT11_TKIPMIC_FAILURE_PARAMETERS;

typedef struct DOT11_PMKID_CANDIDATE_LIST_PARAMETERS {
    #define DOT11_PMKID_CANDIDATE_LIST_PARAMETERS_REVISION_1  1
    NDIS_OBJECT_HEADER Header;
    ULONG uCandidateListSize;
    ULONG uCandidateListOffset;
} DOT11_PMKID_CANDIDATE_LIST_PARAMETERS, * PDOT11_PMKID_CANDIDATE_LIST_PARAMETERS;

typedef struct DOT11_BSSID_CANDIDATE {
    DOT11_MAC_ADDRESS BSSID;

    #define DOT11_PMKID_CANDIDATE_PREAUTH_ENABLED   0x00000001U
    ULONG uFlags;
} DOT11_BSSID_CANDIDATE, *PDOT11_BSSID_CANDIDATE;

typedef struct DOT11_PHY_STATE_PARAMETERS {
    #define DOT11_PHY_STATE_PARAMETERS_REVISION_1  1
    NDIS_OBJECT_HEADER Header;
    ULONG uPhyId;
    BOOLEAN bHardwarePhyState;
    BOOLEAN bSoftwarePhyState;
} DOT11_PHY_STATE_PARAMETERS, * PDOT11_PHY_STATE_PARAMETERS;

typedef struct DOT11_LINK_QUALITY_ENTRY {
    DOT11_MAC_ADDRESS PeerMacAddr;
    UCHAR ucLinkQuality;
} DOT11_LINK_QUALITY_ENTRY, *PDOT11_LINK_QUALITY_ENTRY;

typedef struct DOT11_LINK_QUALITY_PARAMETERS {
    #define DOT11_LINK_QUALITY_PARAMETERS_REVISION_1  1
    NDIS_OBJECT_HEADER Header;
    ULONG uLinkQualityListSize;
    ULONG uLinkQualityListOffset;
} DOT11_LINK_QUALITY_PARAMETERS, * PDOT11_LINK_QUALITY_PARAMETERS;


// Send OOB data for ExtSTA mode
typedef struct DOT11_EXTSTA_SEND_CONTEXT {
    #define DOT11_EXTSTA_SEND_CONTEXT_REVISION_1  1
    NDIS_OBJECT_HEADER Header;
    USHORT usExemptionActionType;
    ULONG uPhyId;
    ULONG uDelayedSleepValue;

#ifdef __midl
    // For nwifi test, which pass this structure using midl
    ULONG_PTR pvMediaSpecificInfo;
#else
    PVOID pvMediaSpecificInfo;
#endif

    ULONG uSendFlags;           // reserved field for safe mode wireless
} DOT11_EXTSTA_SEND_CONTEXT, * PDOT11_EXTSTA_SEND_CONTEXT;

// Recv OOB data for ExtSTA mode
#define DOT11_RECV_FLAG_RAW_PACKET	        0x00000001U
#define DOT11_RECV_FLAG_RAW_PACKET_FCS_FAILURE  0x00000002U
#define DOT11_RECV_FLAG_RAW_PACKET_TIMESTAMP    0x00000004U
typedef struct DOT11_EXTSTA_RECV_CONTEXT {
    #define DOT11_EXTSTA_RECV_CONTEXT_REVISION_1  1
    NDIS_OBJECT_HEADER Header;
    ULONG uReceiveFlags;
    ULONG uPhyId;
    ULONG uChCenterFrequency;
    USHORT usNumberOfMPDUsReceived;
    LONG lRSSI;
    UCHAR ucDataRate;

    ULONG uSizeMediaSpecificInfo;

#ifdef __midl
    // For nwifi test, which pass this structure using midl
    ULONG_PTR pvMediaSpecificInfo;
#else
    PVOID pvMediaSpecificInfo;
#endif

    ULONGLONG ullTimestamp;

} DOT11_EXTSTA_RECV_CONTEXT, * PDOT11_EXTSTA_RECV_CONTEXT;

//
// Private 802.11 OIDs: this should be the last section
//
// We reserve 1024 entries for real DOT11 OIDs
//

#define OID_DOT11_PRIVATE_OIDS_START                (OID_DOT11_NDIS_START + 1024)

#define OID_DOT11_CURRENT_ADDRESS                   (OID_DOT11_PRIVATE_OIDS_START + 2)
    // DOT11_MAC_ADDRESS

#define OID_DOT11_PERMANENT_ADDRESS                 (OID_DOT11_PRIVATE_OIDS_START + 3)
    // DOT11_MAC_ADDRESS

#define OID_DOT11_MULTICAST_LIST                    (OID_DOT11_PRIVATE_OIDS_START + 4)
    // OID_802_3_MULTICAST_LIST

#define OID_DOT11_MAXIMUM_LIST_SIZE                 (OID_DOT11_PRIVATE_OIDS_START + 5)

/* inlined from ndis.h */
//
// 802.11 specific status indication codes
//
#define NDIS_STATUS_DOT11_SCAN_CONFIRM                  ((NDIS_STATUS)0x40030000L)
#define NDIS_STATUS_DOT11_MPDU_MAX_LENGTH_CHANGED       ((NDIS_STATUS)0x40030001L)
#define NDIS_STATUS_DOT11_ASSOCIATION_START             ((NDIS_STATUS)0x40030002L)
#define NDIS_STATUS_DOT11_ASSOCIATION_COMPLETION        ((NDIS_STATUS)0x40030003L)
#define NDIS_STATUS_DOT11_CONNECTION_START              ((NDIS_STATUS)0x40030004L)
#define NDIS_STATUS_DOT11_CONNECTION_COMPLETION         ((NDIS_STATUS)0x40030005L)
#define NDIS_STATUS_DOT11_ROAMING_START                 ((NDIS_STATUS)0x40030006L)
#define NDIS_STATUS_DOT11_ROAMING_COMPLETION            ((NDIS_STATUS)0x40030007L)
#define NDIS_STATUS_DOT11_DISASSOCIATION                ((NDIS_STATUS)0x40030008L)
#define NDIS_STATUS_DOT11_TKIPMIC_FAILURE               ((NDIS_STATUS)0x40030009L)
#define NDIS_STATUS_DOT11_PMKID_CANDIDATE_LIST          ((NDIS_STATUS)0x4003000AL)
#define NDIS_STATUS_DOT11_PHY_STATE_CHANGED             ((NDIS_STATUS)0x4003000BL)
#define NDIS_STATUS_DOT11_LINK_QUALITY                  ((NDIS_STATUS)0x4003000CL)

#define NDIS_STATUS_DOT11_MEDIA_IN_USE          ((NDIS_STATUS)0xC0232001L)
#define NDIS_STATUS_DOT11_POWER_STATE_INVALID   ((NDIS_STATUS)0xC0232002L)

#endif

/*
 * Provide OSL equivalents to NDIS utility functions defined in ndis.h.
 */

#include <osl.h>

#define NdisMoveMemory(dst, src, len)	bcopy((src), (dst), (len))
#define NdisEqualMemory(b1, b2, len)	(!bcmp((b1), (b2), (len)))
#define NdisZeroMemory(b, len)		bzero((b), (len))


/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#endif /* _bcm_ndis_h_ */
