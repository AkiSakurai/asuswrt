#include <stdio.h>
#include <stdlib.h>

#include "tr_lib.h"
#include "ao.h"
#include "spv.h"
#include "device.h"

#include <shared.h>
#include <bcmnvram.h>
#include <bcmutils.h>
#include <shutils.h>

#include <iwlib.h>
#include <sysdeps/ralink/ralink.h>

#define IW15_MAX_FREQUENCIES	16
#define IW15_MAX_BITRATES	8
#define IW15_MAX_TXPOWER	8
#define IW15_MAX_ENCODING_SIZES	8
#define IW15_MAX_SPY		8
#define IW15_MAX_AP		8

struct	iw15_range
{
	__u32		throughput;
	__u32		min_nwid;
	__u32		max_nwid;
	__u16		num_channels;
	__u8		num_frequency;
	struct iw_freq	freq[IW15_MAX_FREQUENCIES];
	__s32		sensitivity;
	struct iw_quality	max_qual;
	__u8		num_bitrates;
	__s32		bitrate[IW15_MAX_BITRATES];
	__s32		min_rts;
	__s32		max_rts;
	__s32		min_frag;
	__s32		max_frag;
	__s32		min_pmp;
	__s32		max_pmp;
	__s32		min_pmt;
	__s32		max_pmt;
	__u16		pmp_flags;
	__u16		pmt_flags;
	__u16		pm_capa;
	__u16		encoding_size[IW15_MAX_ENCODING_SIZES];
	__u8		num_encoding_sizes;
	__u8		max_encoding_tokens;
	__u16		txpower_capa;
	__u8		num_txpower;
	__s32		txpower[IW15_MAX_TXPOWER];
	__u8		we_version_compiled;
	__u8		we_version_source;
	__u16		retry_capa;
	__u16		retry_flags;
	__u16		r_time_flags;
	__s32		min_retry;
	__s32		max_retry;
	__s32		min_r_time;
	__s32		max_r_time;
	struct iw_quality	avg_qual;
};

union	iw_range_raw
{
	struct iw15_range	range15;	/* WE 9->15 */
	struct iw_range		range;		/* WE 16->current */
};

#define iwr15_off(f)	( ((char *) &(((struct iw15_range *) NULL)->f)) - \
			  (char *) NULL)
#define iwr_off(f)	( ((char *) &(((struct iw_range *) NULL)->f)) - \
			  (char *) NULL)

int	iw_ignore_version_sp = 0;

int
ralink_get_range_info(iwrange *	range, char* buffer, int length)
{
  union iw_range_raw *	range_raw;

  /* Point to the buffer */
  range_raw = (union iw_range_raw *) buffer;

  /* For new versions, we can check the version directly, for old versions
   * we use magic. 300 bytes is a also magic number, don't touch... */
  if (length < 300)
    {
      /* That's v10 or earlier. Ouch ! Let's make a guess...*/
      range_raw->range.we_version_compiled = 9;
    }

  /* Check how it needs to be processed */
  if (range_raw->range.we_version_compiled > 15)
    {
      /* This is our native format, that's easy... */
      /* Copy stuff at the right place, ignore extra */
      memcpy((char *) range, buffer, sizeof(iwrange));
    }
  else
    {
      /* Zero unknown fields */
      bzero((char *) range, sizeof(struct iw_range));

      /* Initial part unmoved */
      memcpy((char *) range,
	     buffer,
	     iwr15_off(num_channels));
      /* Frequencies pushed futher down towards the end */
      memcpy((char *) range + iwr_off(num_channels),
	     buffer + iwr15_off(num_channels),
	     iwr15_off(sensitivity) - iwr15_off(num_channels));
      /* This one moved up */
      memcpy((char *) range + iwr_off(sensitivity),
	     buffer + iwr15_off(sensitivity),
	     iwr15_off(num_bitrates) - iwr15_off(sensitivity));
      /* This one goes after avg_qual */
      memcpy((char *) range + iwr_off(num_bitrates),
	     buffer + iwr15_off(num_bitrates),
	     iwr15_off(min_rts) - iwr15_off(num_bitrates));
      /* Number of bitrates has changed, put it after */
      memcpy((char *) range + iwr_off(min_rts),
	     buffer + iwr15_off(min_rts),
	     iwr15_off(txpower_capa) - iwr15_off(min_rts));
      /* Added encoding_login_index, put it after */
      memcpy((char *) range + iwr_off(txpower_capa),
	     buffer + iwr15_off(txpower_capa),
	     iwr15_off(txpower) - iwr15_off(txpower_capa));
      /* Hum... That's an unexpected glitch. Bummer. */
      memcpy((char *) range + iwr_off(txpower),
	     buffer + iwr15_off(txpower),
	     iwr15_off(avg_qual) - iwr15_off(txpower));
      /* Avg qual moved up next to max_qual */
      memcpy((char *) range + iwr_off(avg_qual),
	     buffer + iwr15_off(avg_qual),
	     sizeof(struct iw_quality));
    }

  /* We are now checking much less than we used to do, because we can
   * accomodate more WE version. But, there are still cases where things
   * will break... */
  if (!iw_ignore_version_sp)
    {
      /* We don't like very old version (unfortunately kernel 2.2.X) */
      if (range->we_version_compiled <= 10)
	{
	  fprintf(stderr, "Warning: Driver for device %s has been compiled with an ancient version\n", "raxx");
	  fprintf(stderr, "of Wireless Extension, while this program support version 11 and later.\n");
	  fprintf(stderr, "Some things may be broken...\n\n");
	}

      /* We don't like future versions of WE, because we can't cope with
       * the unknown */
      if (range->we_version_compiled > WE_MAX_VERSION)
	{
	  fprintf(stderr, "Warning: Driver for device %s has been compiled with version %d\n", "raxx", range->we_version_compiled);
	  fprintf(stderr, "of Wireless Extension, while this program supports up to version %d.\n", WE_VERSION);
	  fprintf(stderr, "Some things may be broken...\n\n");
	}

      /* Driver version verification */
      if ((range->we_version_compiled > 10) &&
	 (range->we_version_compiled < range->we_version_source))
	{
	  fprintf(stderr, "Warning: Driver for device %s recommend version %d of Wireless Extension,\n", "raxx", range->we_version_source);
	  fprintf(stderr, "but has been compiled with version %d, therefore some driver features\n", range->we_version_compiled);
	  fprintf(stderr, "may not be available...\n\n");
	}
      /* Note : we are only trying to catch compile difference, not source.
       * If the driver source has not been updated to the latest, it doesn't
       * matter because the new fields are set to zero */
    }

  /* Don't complain twice.
   * In theory, the test apply to each individual driver, but usually
   * all drivers are compiled from the same kernel. */
  iw_ignore_version_sp = 1;

  return (0);
}

int wl_control_channel(int unit)
{
	int ret = 0;
	int channel;
	struct iw_range	range;
	double freq;
	struct iwreq wrq0;
	struct iwreq wrq1;
	struct iwreq wrq2;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_", *ifname;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	ifname = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	if(wl_ioctl(ifname, SIOCGIWAP, &wrq0) < 0)
	{
		return 0;
	}

	wrq0.u.ap_addr.sa_family = ARPHRD_ETHER;

	if (wl_ioctl(ifname, SIOCGIWFREQ, &wrq1) < 0)
		return 0;

	char buffer[sizeof(iwrange) * 2];
	bzero(buffer, sizeof(buffer));
	wrq2.u.data.pointer = (caddr_t) buffer;
	wrq2.u.data.length = sizeof(buffer);
	wrq2.u.data.flags = 0;

	if (wl_ioctl(ifname, SIOCGIWRANGE, &wrq2) < 0)
		return ret;

	if (ralink_get_range_info(&range, buffer, wrq2.u.data.length) < 0)
		return ret;

	freq = iw_freq2float(&(wrq1.u.freq));
	if(freq < KILO)
		channel = (int) freq;
	else
	{
		channel = iw_freq_to_channel(freq, &range);
		if (channel < 0)
			return 0;
	}

	return channel;
}

int get_wireless_totalassociations(char *path)
{
	struct iwreq wrq3;
	char prefix[sizeof("wlXXXXXXXXXX_")], tmp[32];
	
	char data[16384];
	memset(data, 0, sizeof(data));
	wrq3.u.data.pointer = data;
	wrq3.u.data.length = sizeof(data);
	wrq3.u.data.flags = 0;
	int r;

#ifdef TR098
	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));
#endif
#ifdef TR181
	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
#endif

	char *ifname = nvram_safe_get(strcat_r(prefix, "ifname", tmp));
	
	if ( strlen(ifname) == 0 )
		return 0;

	if ((r = wl_ioctl(ifname, RTPRIV_IOCTL_GET_MAC_TABLE, &wrq3)) < 0) {
		return 0;
	}

	RT_802_11_MAC_TABLE_5G* mp =(RT_802_11_MAC_TABLE_5G*)wrq3.u.data.pointer;
	RT_802_11_MAC_TABLE_2G* mp2=(RT_802_11_MAC_TABLE_2G*)wrq3.u.data.pointer;

	if (!strcmp(ifname, WIF_2G))
		return mp2->Num;
	else if (!strcmp(ifname, WIF_5G))
		return mp->Num;

	return 0;
}

int renew_associatation(char *path)
{
	int i;
	char buf[256];
	int count;
	node_t node;
	node_t *children = NULL;

	int instance_num;
	
	if(strstr(path, TOTALASSOCIATE) == NULL)
		return -1;

	instance_num = atoi(path + strlen(TOTALASSOCIATE));//get instance num

	if((strncmp(path, TOTALASSOCIATE, strlen(TOTALASSOCIATE)) == 0) && (strstr(path, "AssociatedDevice.") != NULL)) {
		int associatednum = get_wireless_totalassociations(path);//get wireless client totalnum

		sprintf(buf, "%s%d.AssociatedDevice", TOTALASSOCIATE, instance_num);
		if(associatednum == 0) {
			//sprintf(buf, "%s%d.AssociatedDevice", TOTALASSOCIATE, instance_num);
			delete_all_instance(buf);
		}
		else {
			if(lib_resolve_node(buf, &node) == 0) {
				count = lib_get_children(node, &children);
				
				if(count != associatednum || count == associatednum) { //update macaddr
					sprintf(buf, "%s%d.AssociatedDevice", TOTALASSOCIATE, instance_num);
					delete_all_instance(buf);
					
					for(i = 0; i < associatednum; i++) {
						sprintf(buf, "%s%d.AssociatedDevice.", TOTALASSOCIATE, instance_num);
						if(add_object(buf, strlen(buf)) < 9000) {
#ifdef TR098
							sprintf(buf, "%s%d.AssociatedDevice.%d.AssociatedDeviceMACAddress", TOTALASSOCIATE, instance_num, i + 1);
#else
				        		sprintf(buf, "%s%d.AssociatedDevice.%d.MACAddress", TOTALASSOCIATE, instance_num, i + 1);
#endif

							/* set value*/
{
							struct iwreq wrq3;
							char prefix[sizeof("wlXXXXXXXXXX_")], tmp[32];
	
							char data[16384];
							memset(data, 0, sizeof(data));
							wrq3.u.data.pointer = data;
							wrq3.u.data.length = sizeof(data);
							wrq3.u.data.flags = 0;
							int r;

						#ifdef TR098
							snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));
						#endif
						#ifdef TR181
							snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
						#endif

							char *ifname = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

							if ((r = wl_ioctl(ifname, RTPRIV_IOCTL_GET_MAC_TABLE, &wrq3)) < 0) {
								return 0;
							}

							RT_802_11_MAC_TABLE_5G* mp =(RT_802_11_MAC_TABLE_5G*)wrq3.u.data.pointer;
							RT_802_11_MAC_TABLE_2G* mp2=(RT_802_11_MAC_TABLE_2G*)wrq3.u.data.pointer;

							char mac[20];
							if (!strcmp(ifname, WIF_2G)) {
								RT_802_11_MAC_ENTRY_for_2G *Entry = ((RT_802_11_MAC_ENTRY_for_2G *)(mp2->Entry)) + i;
								sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", Entry->Addr[0],Entry->Addr[1],Entry->Addr[2],Entry->Addr[3],Entry->Addr[4],Entry->Addr[5]);
								__set_parameter_value(buf, mac);
							}
							else if (!strcmp(ifname, WIF_5G)) {
								RT_802_11_MAC_ENTRY_for_5G *Entry = ((RT_802_11_MAC_ENTRY_for_5G *)(mp->Entry)) + i;
								sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", Entry->Addr[0],Entry->Addr[1],Entry->Addr[2],Entry->Addr[3],Entry->Addr[4],Entry->Addr[5]);
								__set_parameter_value(buf, mac);
							}
}
						/* set value*/
							
						}
					}
				}
			}
		}
	}

	return 0;
}


