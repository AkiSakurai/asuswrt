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
#include <sys/socket.h>
#include <linux/types.h>

#include "tr_lib.h"
#include "ao.h"
#include "spv.h"
#include "tr_strings.h"
#include "device.h"

#include <shared.h>
#include <shutils.h>

#include "libtcapi.h"
#include <tcutils.h> 

/* --------------------------- SUBTYPES --------------------------- */
/*
 *	Generic format for most parameters that fit in an int
 */
struct	iw_param
{
  __s32		value;		/* The value of the parameter itself */
  __u8		fixed;		/* Hardware should not use auto select */
  __u8		disabled;	/* Disable the feature */
  __u16		flags;		/* Various specifc flags (if any) */
};

/*
 *	For all data larger than 16 octets, we need to use a
 *	pointer to memory allocated in user space.
 */
struct	iw_point
{
  void __user	*pointer;	/* Pointer to the data  (in user space) */
  __u16		length;		/* number of fields or size in bytes */
  __u16		flags;		/* Optional params */
};

/*
 *	A frequency
 *	For numbers lower than 10^9, we encode the number in 'm' and
 *	set 'e' to 0
 *	For number greater than 10^9, we divide it by the lowest power
 *	of 10 to get 'm' lower than 10^9, with 'm'= f / (10^'e')...
 *	The power of 10 is in 'e', the result of the division is in 'm'.
 */
struct	iw_freq
{
	__s32		m;		/* Mantissa */
	__s16		e;		/* Exponent */
	__u8		i;		/* List index (when in range struct) */
	__u8		flags;		/* Flags (fixed/auto) */
};

/*
 *	Quality of the link
 */
struct	iw_quality
{
	__u8		qual;		/* link quality (%retries, SNR,
					   %missed beacons or better...) */
	__u8		level;		/* signal level (dBm) */
	__u8		noise;		/* noise level (dBm) */
	__u8		updated;	/* Flags to know if updated */
};

/* ------------------------ IOCTL REQUEST ------------------------ */
/*
 * This structure defines the payload of an ioctl, and is used 
 * below.
 *
 * Note that this structure should fit on the memory footprint
 * of iwreq (which is the same as ifreq), which mean a max size of
 * 16 octets = 128 bits. Warning, pointers might be 64 bits wide...
 * You should check this when increasing the structures defined
 * above in this file...
 */
union	iwreq_data
{
	/* Config - generic */
	char		name[IFNAMSIZ];
	/* Name : used to verify the presence of  wireless extensions.
	 * Name of the protocol/provider... */

	struct iw_point	essid;		/* Extended network name */
	struct iw_param	nwid;		/* network id (or domain - the cell) */
	struct iw_freq	freq;		/* frequency or channel :
					 * 0-1000 = channel
					 * > 1000 = frequency in Hz */

	struct iw_param	sens;		/* signal level threshold */
	struct iw_param	bitrate;	/* default bit rate */
	struct iw_param	txpower;	/* default transmit power */
	struct iw_param	rts;		/* RTS threshold threshold */
	struct iw_param	frag;		/* Fragmentation threshold */
	__u32		mode;		/* Operation mode */
	struct iw_param	retry;		/* Retry limits & lifetime */

	struct iw_point	encoding;	/* Encoding stuff : tokens */
	struct iw_param	power;		/* PM duration/timeout */
	struct iw_quality qual;		/* Quality part of statistics */

	struct sockaddr	ap_addr;	/* Access point address */
	struct sockaddr	addr;		/* Destination address (hw/mac) */

	struct iw_param	param;		/* Other small parameters */
	struct iw_point	data;		/* Other large parameters */
};

/*
 * The structure to exchange data for ioctl.
 * This structure is the same as 'struct ifreq', but (re)defined for
 * convenience...
 * Do I need to remind you about structure size (32 octets) ?
 */
struct	iwreq 
{
	union
	{
		char	ifrn_name[IFNAMSIZ];	/* if name, e.g. "eth0" */
	} ifr_ifrn;

	/* Data part (defined just above) */
	union	iwreq_data	u;
};

#define SIOCIWFIRSTPRIV			0x8BE0
#define RTPRIV_IOCTL_GET_MAC_TABLE					(SIOCIWFIRSTPRIV + 0x0F)
#define MAX_LEN_OF_MAC_TABLE 32
#define MAC_ADDR_LENGTH                 6

 // MIMO Tx parameter, ShortGI, MCS, STBC, etc.  these are fields in TXWI. Don't change this definition!!!
typedef union  _MACHTTRANSMIT_SETTING {
	struct	{
	unsigned short   	MCS:7;                 // MCS
	unsigned short		BW:1;	//channel bandwidth 20MHz or 40 MHz
	unsigned short		ShortGI:1;
	unsigned short		STBC:2;	//SPACE 
	unsigned short		rsv:3;	 
	unsigned short		MODE:2;	// Use definition MODE_xxx.  
	}	field;
	unsigned short		word;
 } MACHTTRANSMIT_SETTING, *PMACHTTRANSMIT_SETTING;

typedef struct _RT_802_11_MAC_ENTRY {
#if defined(RT5392)||defined(MT7592)
		unsigned char       ApIdx;
#endif
     	unsigned char       Addr[MAC_ADDR_LENGTH];
     	unsigned char       Aid;
     	unsigned char       Psm;     //used for ssidindex
     	unsigned char		MimoPs;  // 0:MMPS_STATIC, 1:MMPS_DYNAMIC, 3:MMPS_Enabled
   	signed char		AvgRssi0;
	signed char		AvgRssi1;
	signed char		AvgRssi2;
	unsigned int		ConnectedTime;
    MACHTTRANSMIT_SETTING	TxRate;
} RT_802_11_MAC_ENTRY, *PRT_802_11_MAC_ENTRY;

typedef struct _RT_802_11_MAC_TABLE {
    unsigned long       Num;
    RT_802_11_MAC_ENTRY Entry[MAX_LEN_OF_MAC_TABLE];
} RT_802_11_MAC_TABLE, *PRT_802_11_MAC_TABLE;

#define ETHER_ADDR_STR_LEN	18

int wl_control_channel(int unit)
{
	FILE *fp;
	char buf[256]={0}, channel[4]={0}, wifi_if[8]={0};
#if defined(TCSUPPORT_WLAN_RT6856)
	char freq[8]={0};
#endif
	char *pValue=NULL;
	int chan=0;

#if defined(TCSUPPORT_WLAN_RT6856)	//iNIC
	if(unit == 0){
		strcpy(wifi_if, "ra00_0");
	}
	else{
		strcpy(wifi_if, "ra01_0");
	}
#else	//Non-iNIC
#if defined(TCSUPPORT_DUAL_WLAN)	//dual band.
	if(unit == 0){
		strcpy(wifi_if, "ra0");
	}
	else{
		strcpy(wifi_if, "rai0");
	}
#else
	strcpy(wifi_if, "ra0");
#endif
#endif

	sprintf(buf, "/userfs/bin/iwconfig %s > /tmp/var/iwconfig.tmp", wifi_if);
	system(buf);
	fp = fopen("/tmp/var/iwconfig.tmp","r");
	if(fp != NULL){
		while(fgets(buf,256,fp)){
#if defined(TCSUPPORT_WLAN_RT6856)	//iNIC
			if((pValue=strstr(buf,"Frequency:"))){
				/*tmpBuf format: Frequency:5.32 GHz*/
				pValue += strlen("Frequency:");
				sscanf(pValue,"%s", freq);
				if(unit){	//5G
					chan = ((int)(atof(freq)*1000)-5000)/5;
				}
				else{	// 2G
					chan = ( ((int)(atof(freq)*1000)-2412)/5)+1;
				}
			}
#else	//Non-iNIC
			if((pValue=strstr(buf,"Channel="))){
				/*tmpBuf format: Channel=157*/
				pValue += strlen("Channel=");
				sscanf(pValue,"%s", channel);
				chan = atoi(channel);
			}
#endif
		}
		fclose(fp);
		unlink("/tmp/var/iwconfig.tmp");
		return chan;
	}
	else{
		printf("Get channel fail!!\n");
		return 0;
	}
}

RT_802_11_MAC_TABLE *get_wireless_totalclient(char *path)
{
	char prefix[sizeof("wlXXXXXXXXXX_")], tmp[32];
	int socket_id = 0;
	struct iwreq wrq;
	int ret = 0;
	char data[16384] = {0};

#ifdef TR098
	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));
#endif
#ifdef TR181
	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
#endif

	/* for mutil-ssid do not enable, will print dev error:: "cmd=159: No such device" */
	if(strlen(prefix) > 4) {
		if(tcapi_get_int(WLAN_NODE, strcat_r(prefix, "bss_enabled", tmp)) == 0)
			return NULL;
	}

	socket_id = socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_id == -1) {
		printf("==>wlan_read:Create ACL socket fail\n");
		return NULL;
	}

	memset(tmp, 0, sizeof(tmp));
	memset(data, 0, sizeof(data));

	strcpy(wrq.ifr_name, get_wifi_ifname(prefix, tmp));
	wrq.u.data.length = sizeof(data);
	wrq.u.data.pointer = (caddr_t)data;
	wrq.u.data.flags = 0;
	ret = ioctl(socket_id, RTPRIV_IOCTL_GET_MAC_TABLE, &wrq);

	if (ret != 0) {
		printf("==>wlan_read:ioctl open fail\n");
		close(socket_id);
		return NULL;
	}
	close(socket_id);

	RT_802_11_MAC_TABLE *mp = (RT_802_11_MAC_TABLE*)wrq.u.data.pointer;

	return mp;
}

int get_wireless_totalassociations(char *path)
{
	RT_802_11_MAC_TABLE *client = get_wireless_totalclient(path);
	return (client ? client->Num : 0);
}

int renew_associatation(char *path)
{
	int i;
	int changed_flag = 0;
	char buf[256];
	char ea[ETHER_ADDR_STR_LEN];
	int count;
	node_t node;
	node_t *children = NULL;

	int instance_num;
	
	if(strstr(path, TOTALASSOCIATE) == NULL)
		return -1;

	instance_num = atoi(path + strlen(TOTALASSOCIATE));//get instance num

	if((strncmp(path, TOTALASSOCIATE, strlen(TOTALASSOCIATE)) == 0) && (strstr(path, "AssociatedDevice.") != NULL)) {
		RT_802_11_MAC_TABLE * client_list = get_wireless_totalclient(path);//get wireless client totalnum

		if(client_list == NULL)
			return -1;

		sprintf(buf, "%s%d.AssociatedDevice", TOTALASSOCIATE, instance_num);
		if(client_list->Num == 0) {
			//sprintf(buf, "%s%d.AssociatedDevice", TOTALASSOCIATE, instance_num);
			delete_all_instance(buf);
		}
		else {
			if(lib_resolve_node(buf, &node) == 0) {
				count = lib_get_children(node, &children);

				if(count != client_list->Num) {
					sprintf(buf, "%s%d.AssociatedDevice", TOTALASSOCIATE, instance_num);
					delete_all_instance(buf);

					for(i = 0; i < client_list->Num; i++) {
						sprintf(buf, "%s%d.AssociatedDevice.", TOTALASSOCIATE, instance_num);
						if(add_object(buf, strlen(buf)) < 9000) {
#ifdef TR098
							sprintf(buf, "%s%d.AssociatedDevice.%d.AssociatedDeviceMACAddress", TOTALASSOCIATE, instance_num, i + 1);
#else
				        		sprintf(buf, "%s%d.AssociatedDevice.%d.MACAddress", TOTALASSOCIATE, instance_num, i + 1);
#endif
				        		__set_parameter_value(buf, ether_etoa((void *)&client_list->Entry[i], ea));
						}
					}
				}
				else {
					i = 0;
					char buf[128], buf_mac[32];
					while(count > 0) {
						char name[16];
						char tmp_s[64];
						count--;
						if(lib_get_property(children[count], "name", name) == 0) {
		    					if(string_is_digits(name) == 1) {
#ifdef TR098
							sprintf(buf, "%s%d.AssociatedDevice.%d.AssociatedDeviceMACAddress", TOTALASSOCIATE, instance_num, i + 1);
#else
				        		sprintf(buf, "%s%d.AssociatedDevice.%d.MACAddress", TOTALASSOCIATE, instance_num, i + 1);
#endif
								sprintf(buf_mac, "%s", __get_parameter_value(buf, tmp_s));

								if(strcmp(buf_mac, ether_etoa((void *)&client_list->Entry[i], ea)) != 0) {
									changed_flag = 1;
									break;
								}
							}
						}
						i++;
					}

					if(changed_flag == 1) {
						sprintf(buf, "%s%d.AssociatedDevice", TOTALASSOCIATE, instance_num);
						delete_all_instance(buf);

						for(i = 0; i < client_list->Num; i++) {
							sprintf(buf, "%s%d.AssociatedDevice.", TOTALASSOCIATE, instance_num);
							if(add_object(buf, strlen(buf)) < 9000) {
#ifdef TR098
							sprintf(buf, "%s%d.AssociatedDevice.%d.AssociatedDeviceMACAddress", TOTALASSOCIATE, instance_num, i + 1);
#else
				        		sprintf(buf, "%s%d.AssociatedDevice.%d.MACAddress", TOTALASSOCIATE, instance_num, i + 1);
#endif

				        			__set_parameter_value(buf, ether_etoa((void *)&client_list->Entry[i], ea));
							}
						}
					}
				}
			}
			else
				return -1;
		}
	}
	return 0;
}
