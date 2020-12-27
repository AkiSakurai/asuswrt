/*
 * Host AP (software wireless LAN access point) user space daemon for
 * Host AP kernel driver / common helper functions, etc.
 * Copyright 2002-2003, Jouni Malinen <jkmaline@cc.hut.fi>
 * Copyright 2003, Instant802 Networks, Inc.
 * All Rights Reserved.
 */
/* $Copyright... $
 *
 * $Id: common.h 663931 2016-10-07 11:45:25Z $
 */

#ifndef COMMON_H
#define COMMON_H
#include <typedefs.h>
#include <endian.h>
#include <byteswap.h>
#include <wlioctl.h>
#include <wlif_utils.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define le_to_host16(n) (n)
#define host_to_le16(n) (n)
#define be_to_host16(n) bswap_16(n)
#define host_to_be16(n) bswap_16(n)
#define le_to_host64(n) (n)
#define host_to_le64(n) (n)
#define be_to_host64(n) bswap_64(n)
#define host_to_be64(n) bswap_64(n)
#else
#define le_to_host16(n) bswap_16(n)
#define host_to_le16(n) bswap_16(n)
#define be_to_host16(n) (n)
#define host_to_be16(n) (n)
#define le_to_host64(n) bswap_64(n)
#define host_to_le64(n) bswap_64(n)
#define be_to_host64(n) (n)
#define host_to_be64(n) (n)
#endif /* __LITTLE_ENDIAN */

#ifndef ETH_P_ALL
#define ETH_P_ALL 0x0003
#endif
#ifndef ETH_P_IP
#define ETH_P_IP 0x0800
#endif

#ifndef ETH_P_RRB
#define ETH_P_RRB 0x890D
#endif /* ETH_P_RRB */

#define STRUCT_PACKED __attribute__ ((packed))
/* Macros for handling unaligned memory accesses */

#define WPA_GET_BE16(a) ((uint16) (((a)[0] << 8) | (a)[1]))

#define WPA_PUT_BE16(a, val)                    \
        do {                                    \
                (a)[0] = ((uint16) (val)) >> 8;    \
                (a)[1] = ((uint16) (val)) & 0xff;  \
        } while (0)

#define WPA_GET_LE16(a) ((uint16) (((a)[1] << 8) | (a)[0]))

#define WPA_PUT_LE16(a, val)                    \
        do {                                    \
                (a)[1] = ((uint16) (val)) >> 8;    \
                (a)[0] = ((uint16) (val)) & 0xff;  \
        } while (0)

#define WPA_GET_BE24(a) ((((uint32) (a)[0]) << 16) | (((uint32) (a)[1]) << 8) | \
                         ((uint32) (a)[2]))
#define WPA_PUT_BE24(a, val)                                    \
        do {                                                    \
                (a)[0] = (uint8) ((((uint32) (val)) >> 16) & 0xff);   \
                (a)[1] = (uint8) ((((uint32) (val)) >> 8) & 0xff);    \
                (a)[2] = (uint8) (((uint32) (val)) & 0xff);           \
        } while (0)

#define WPA_GET_BE32(a) ((((uint32) (a)[0]) << 24) | (((uint32) (a)[1]) << 16) | \
                         (((uint32) (a)[2]) << 8) | ((uint32) (a)[3]))
#define WPA_PUT_BE32(a, val)                                    \
        do {                                                    \
                (a)[0] = (uint8) ((((uint32) (val)) >> 24) & 0xff);   \
                (a)[1] = (uint8) ((((uint32) (val)) >> 16) & 0xff);   \
                (a)[2] = (uint8) ((((uint32) (val)) >> 8) & 0xff);    \
                (a)[3] = (uint8) (((uint32) (val)) & 0xff);           \
        } while (0)

#define WPA_GET_LE32(a) ((((uint32) (a)[3]) << 24) | (((uint32) (a)[2]) << 16) | \
                         (((uint32) (a)[1]) << 8) | ((uint32) (a)[0]))
#define WPA_PUT_LE32(a, val)                                    \
        do {                                                    \
                (a)[3] = (uint8) ((((uint32) (val)) >> 24) & 0xff);   \
                (a)[2] = (uint8) ((((uint32) (val)) >> 16) & 0xff);   \
                (a)[1] = (uint8) ((((uint32) (val)) >> 8) & 0xff);    \
                (a)[0] = (uint8) (((uint32) (val)) & 0xff);           \
        } while (0)

#define WPA_GET_BE64(a) ((((uint64) (a)[0]) << 56) | (((uint64) (a)[1]) << 48) | \
                         (((uint64) (a)[2]) << 40) | (((uint64) (a)[3]) << 32) | \
                         (((uint64) (a)[4]) << 24) | (((uint64) (a)[5]) << 16) | \
                         (((uint64) (a)[6]) << 8) | ((uint64) (a)[7]))
#define WPA_PUT_BE64(a, val)                            \
        do {                                            \
                (a)[0] = (uint8) (((uint64) (val)) >> 56);    \
                (a)[1] = (uint8) (((uint64) (val)) >> 48);    \
                (a)[2] = (uint8) (((uint64) (val)) >> 40);    \
                (a)[3] = (uint8) (((uint64) (val)) >> 32);    \
                (a)[4] = (uint8) (((uint64) (val)) >> 24);    \
                (a)[5] = (uint8) (((uint64) (val)) >> 16);    \
                (a)[6] = (uint8) (((uint64) (val)) >> 8);     \
                (a)[7] = (uint8) (((uint64) (val)) & 0xff);   \
        } while (0)

#define WPA_GET_LE64(a) ((((uint64) (a)[7]) << 56) | (((uint64) (a)[6]) << 48) | \
                         (((uint64) (a)[5]) << 40) | (((uint64) (a)[4]) << 32) | \
                         (((uint64) (a)[3]) << 24) | (((uint64) (a)[2]) << 16) | \
                         (((uint64) (a)[1]) << 8) | ((uint64) (a)[0]))

#define WPA_CIPHER_NONE_BIT BIT(0)
#define WPA_CIPHER_WEP40_BIT BIT(1)
#define WPA_CIPHER_WEP104_BIT BIT(2)
#define WPA_CIPHER_TKIP_BIT BIT(3)
#define WPA_CIPHER_CCMP_BIT BIT(4)
#ifdef MFP
#define WPA_CIPHER_AES_128_CMAC_BIT BIT(5)
#endif /* MFP */


#define WPA_KEY_MGMT_IEEE8021X BIT(0)
#define WPA_KEY_MGMT_PSK BIT(1)
#define WPA_KEY_MGMT_NONE BIT(2)
#define WPA_KEY_MGMT_IEEE8021X_NO_WPA BIT(3)
#define WPA_KEY_MGMT_WPA_NONE BIT(4)
#define WPA_KEY_MGMT_FT_IEEE8021X BIT(5)
#define WPA_KEY_MGMT_FT_PSK BIT(6)
#define WPA_KEY_MGMT_IEEE8021X_SHA256 BIT(7)
#define WPA_KEY_MGMT_PSK_SHA256 BIT(8)


#define WPA_PROTO_WPA BIT(0)
#define WPA_PROTO_RSN BIT(1)

#define WPA_MAX_SSID_LEN 32
/* IEEE 802.11r */
#define MOBILITY_DOMAIN_ID_LEN 2
#define FT_R0KH_ID_MAX_LEN 48
#define FT_R1KH_ID_LEN 6
#define R0_KEY_HOLDER_LEN 1
#define WPA_PMK_NAME_LEN 16
#define PMK_LEN 32
#define KH_KEY_LEN 16
#define MAX_FBTAPS 2

typedef struct fbt {
	char prefix[IFNAMSIZ];		/* instance of interface */
	unsigned char ssid[WPA_MAX_SSID_LEN];
	size_t ssid_len;
	unsigned char mobility_domain[MOBILITY_DOMAIN_ID_LEN];
	unsigned char r0_key_holder[FT_R0KH_ID_MAX_LEN];
	size_t r0_key_holder_len;
	unsigned char r1_key_holder[FT_R1KH_ID_LEN];
	unsigned int r0_key_lifetime;
	unsigned int reassociation_deadline;
	struct ft_remote_r0kh *r0kh_list;
	struct ft_remote_r1kh *r1kh_list;
	int pmk_r1_push;
	int ft_over_ds;
	unsigned char pmk_r1name[WPA_PMK_NAME_LEN];
	struct wpa_ft_pmk_cache *ft_pmk_cache;
	char fbt_aps[WLC_IOCTL_MEDLEN];
} fbt_t;

typedef struct suppl_ft {
	uint32 ft_completed;
	uint32 pmk_r1_name_valid;
	uint8 xxkey[PMK_LEN]; /* PSK or the second 256 bits of MSK */
	size_t xxkey_len;
	uint8 pmk_r1_name[WPA_PMK_NAME_LEN]; /* PMKR1Name derived from FT Auth * Request */
	uint8 r0kh_id[FT_R0KH_ID_MAX_LEN]; /* R0KH-ID from FT Auth Request */
	size_t r0kh_id_len;
	uint8 sup_pmk_r1_name[WPA_PMK_NAME_LEN]; /* PMKR1Name from EAPOL-Key message 2/4 */
	uint8 *assoc_resp_ftie;
} supp_ft_t;

#endif /* COMMON_H */
