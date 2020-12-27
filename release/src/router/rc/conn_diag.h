#include <stdio.h>
#include <syslog.h>
#include <time.h>
#include <linux/limits.h>

#if defined(RTCONFIG_RALINK)
#include <bcmnvram.h>
#include <ralink.h>
#include <net/ethernet.h>
#include <netinet/ether.h>
#include  <float.h>
#ifdef RTCONFIG_WIRELESSREPEATER
#include <ap_priv.h>
#endif
#elif defined(RTCONFIG_QCA)
#include <bcmnvram.h>
#include <net/ethernet.h>
#include <netinet/ether.h>
#elif defined(RTCONFIG_ALPINE)
#include <bcmnvram.h>
#include <net/ethernet.h>
#include <netinet/ether.h>
#else
#include <wlioctl.h>
#include <bcmendian.h>
#endif
#include <wlutils.h>


#define MAX_RE 2
#define MACF_UP	"%02X:%02X:%02X:%02X:%02X:%02X"
#define ACS_CHANIM_BUF_LEN (2*1024)

#define LOG_DIR "/tmp/asusdebuglog"

#define NORMAL_PERIOD 600 /* second */

#define DIAG_FILE_LOCK		"conn_diag"

enum {
	STATE_PHY_DISCONN = 0,
	STATE_PHY_CONN,
	STATE_MODEM_SCAN,
	STATE_MODEM_RESET,
	STATE_MODEM_NOSIM,
	STATE_MODEM_LOCK_PIN,
	STATE_MODEM_LOCK_PUK,
	STATE_MAX
};

enum {
	DIAGMODE_NONE = 0,
	DIAGMODE_CHKSTA = 1,
	DIAGMODE_SYS_DETECT = 2,
	DIAGMODE_SYS_SETTING = 4,
	DIAGMODE_WIFI_DETECT = 8,
	DIAGMODE_WIFI_SETTING = 16,
	DIAGMODE_STAINFO = 32,
	DIAGMODE_NET_DETECT = 64,
	DIAGMODE_MAX
};

#define DIAG_EVENT_SYS "SYS" // DIAG_EVENT_SYS>node type>IP>MAC>FW version>t-code>AiProtection>USB mode>CTF or Runner/FC disable>got_previous_oops
#define DIAG_EVENT_SYS2 "SYS2" // DIAG_EVENT_SYS2>node type>IP>MAC>CPU freq>MemFree>CPU temperature>2G's temperature>5G's temperature
#define DIAG_EVENT_WIFISYS "WIFISYS" // DIAG_EVENT_WIFISYS>node type>IP>MAC>2G's ifname,2G's chip,2G's country/rev>5G's ifname,5G's chip,5G's country/rev
#ifdef RTCONFIG_BCMARM
#define DIAG_EVENT_WIFISYS2 "WIFISYS2" // DIAG_EVENT_WIFISYS2>node type>IP>MAC>band>band's ifname>band's MAC>band's noise>band's MCS>band's capability>band's subif_count>band's subif_ssid>band's chanim
#else
#define DIAG_EVENT_WIFISYS2 "WIFISYS2" // DIAG_EVENT_WIFISYS2>node type>IP>MAC>band>band's ifname>band's MAC>band's noise>band's MCS>band's capability>band's subif_count>band's subif_ssid
#endif
#define DIAG_EVENT_STAINFO "STAINFO" // DIAG_EVENT_STAINFO>node type>IP>MAC>STA's MAC>STA's Band>STA's RSSI>active>STA's Tx>STA's Rx>STA's Tx byte>STA's Rx byte
#define DIAG_EVENT_WLCE "WLCEVENT" // DIAG_EVENT_WLCE>node type>IP>MAC>STA's MAC>STA's Band>last_act>count_auth>count_deauth>count_assoc>count_disassoc>count_reassoc
#define DIAG_EVENT_TG_ROAMING "TG_ROAMING" // DIAG_EVENT_TG_ROAMING>node type>IP>MAC>STA's MAC>STA's Band>STA's RSSI>timestamp>user_low_rssi>rssi_cnt>idle_period>idle_start
#define DIAG_EVENT_ROAMING "ROAMING" // DIAG_EVENT_ROAMING>node type>IP>MAC>STA's MAC>STA's RSSI>timestamp>candidate_rssi_criteria>candidate's MAC>candidate's RSSI
#define DIAG_EVENT_NET "NET" // DIAG_EVENT_NET>node type>IP>MAC>wan_unit>link_wan>runIP>runMASK>runGATE>got_Route>got Redirect rules>DNS>Ping
#ifdef RTCONFIG_BCMBSD
#define DIAG_EVENT_TG_BSD "TG_BSD" // DIAG_EVENT_TG_BSD>node type>IP>MAC>STA's MAC>timestamp>from_chanspec>to_chanspec>reason
#endif

extern int get_hw_acceleration(char *output, int size);
extern int get_sys_clk(char *output, int size);
extern int get_sys_temp(unsigned int *temp);

extern int get_wifi_chip(char *ifname, char *output, int size);
extern int get_wifi_temp(char *ifname, unsigned int *temp);
extern int get_wifi_country(char *ifname, char *output, int len);
extern int get_wifi_noise(char *ifname, char *output, int len);
extern int get_wifi_mcs(char *ifname, char *output, int len);
#ifdef RTCONFIG_BCMARM
extern int get_bss_info(char *ifname, int *capability); // TODO: non-Brcm needs to do.
extern int get_subif_count(char *target, int *count); // TODO: non-Brcm needs to do.
extern int get_subif_ssid(char *target, char *output, int outputlen); // TODO: non-Brcm needs to do.
extern int get_wifi_chanim(char *ifname, char *output, int outputlen);
#endif
