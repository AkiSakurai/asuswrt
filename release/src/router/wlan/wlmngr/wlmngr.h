/*
* <:copyright-BRCM:2011:proprietary:standard
* 
*    Copyright (c) 2011 Broadcom 
*    All Rights Reserved
* 
*  This program is the proprietary software of Broadcom and/or its
*  licensors, and may only be used, duplicated, modified or distributed pursuant
*  to the terms and conditions of a separate, written license agreement executed
*  between you and Broadcom (an "Authorized License").  Except as set forth in
*  an Authorized License, Broadcom grants no license (express or implied), right
*  to use, or waiver of any kind with respect to the Software, and Broadcom
*  expressly reserves all rights in and to the Software and all intellectual
*  property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU HAVE
*  NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY NOTIFY
*  BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
* 
*  Except as expressly set forth in the Authorized License,
* 
*  1. This program, including its structure, sequence and organization,
*     constitutes the valuable trade secrets of Broadcom, and you shall use
*     all reasonable efforts to protect the confidentiality thereof, and to
*     use this information only in connection with your use of Broadcom
*     integrated circuit products.
* 
*  2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
*     AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
*     WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
*     RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND
*     ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT,
*     FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR
*     COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE
*     TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF USE OR
*     PERFORMANCE OF THE SOFTWARE.
* 
*  3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR
*     ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL,
*     INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY
*     WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
*     IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES;
*     OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE
*     SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS
*     SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY
*     LIMITED REMEDY.
:>
*/

#ifndef __WL_MNGR_H__
#define __WL_MNGR_H__

#include <wlcsm_linux.h>
#include <wlcsm_lib_api.h>
#include <bcmconfig.h>
#define WL_SYNC_TO_MDM	0
#define WL_SYNC_TO_MDM_NVRAM    1
#define WL_SYNC_FROM_MDM	2
#define WL_SYNC_FROM_MDM_TR69C  3	
#define WL_SYNC_FROM_MDM_HTTPD  4	
#define WL_SYNC_NONE		(WL_SYNC_FROM_MDM_HTTPD +1)
#define DEFAULT_PAIR_LEN_BYTE 2
#define PAIR_LEN_BYTE 3

extern int  g_wlmngr_restart_all;
typedef struct {
   WIRELESS_VAR m_wlVar;
   WIRELESS_MSSID_VAR m_wlMssidVar[WL_MAX_NUM_SSID];
   char m_ifcName[WL_MAX_NUM_SSID][WL_SM_SIZE_MAX];
   int numStations;
   int numStationsBSS[WL_MAX_NUM_SSID];
   char bssMacAddr[WL_MAX_NUM_SSID][WL_MID_SIZE_MAX];   
   int wlCurrentChannel;
   int m_bands;
   int maxMbss;
   int mbssSupported;
   int numBss;
   WL_STATION_LIST_ENTRY *stationList;
   int qosInitDone;
   char wlVer[WL_MID_SIZE_MAX];
   
   int aburnSupported;
   int ampduSupported;
   int amsduSupported;
   int wmeSupported;
   bool wlInitDone;
   bool onBridge;
   int m_unit;
   int m_refresh;

   WL_FLT_MAC_ENTRY     *m_tblWdsMac;
   WL_FLT_MAC_ENTRY     *m_tblScanWdsMac;
#ifdef WMF
   int wmfSupported;
#endif
#ifdef DUCATI
   int wlVecSupported;
   int wlIperf;
   int wlVec;
#endif
   int apstaSupported;
   int wlMediaSupported;
} WLAN_ADAPTER_STRUCT;


extern int wl_index;
extern WLAN_ADAPTER_STRUCT *m_instance_wl;
extern int is_smp_system;


/******************************************
Function Declaration
*******************************************/
   void wlmngr_initCfg ( unsigned char dataSync, int idx);
   int wlmngr_alloc( int adapter_cnt );
   void wlmngr_free(void );

   void wlmngr_initNvram(int idx);
   void wlmngr_write_nvram(void);

   void wlmngr_initVars(int idx);
   void wlmngr_init(int idx);
   void wlmngr_unInit(int idx);
   void wlmngr_update_assoc_list(int idx);
   
   void wlmngr_setup(int idx, WL_SETUP_TYPE type);
   void wlmngr_store(int idx);
   void wlmngr_retrieve(int idx, int isDefault);
   void wlmngr_getVar(int idx, char *varName, char *varValue);
   void wlmngr_getVarEx(int idx, int argc, char **argv, char *varValue);   
   void wlmngr_setVar(int idx, char *varName, char *varValue);
   void wlmngr_stopServices(int idx);
   void wlmngr_startServices(int idx);
   void wlmngr_startNas(int idx);
   void wlmngr_startNasOne(int idx, int ssid_idx);
   void wlmngr_stopNas(int idx);   
#ifdef SUPPORT_WSC
   void wlmngr_startWsc(int idx);
   void wlmngr_stopWsc(int idx);
#endif
#ifdef SUPPORT_SES   
   void wlmngr_startSes(int idx);
   void wlmngr_stopSes(int idx);   
   void wlmngr_startSesCl(int idx);
   void wlmngr_stopSesCl(int idx);     
   void wlmngr_clearSesLed(int idx);
#endif



   char *strcat_r( const char *s1, const char *s2, char *buf);

   void wlmngr_wlIfcUp(int idx);
   void wlmngr_wlIfcDown(int idx);

   void wlmngr_scanWdsStart(int idx);
   void wlmngr_scanWdsResult(int idx);
   void wlmngr_scanResult(int idx, int chose);

   void wlmngr_getHwAddr(int idx, char *ifname, char *hwaddr);
   int wlmngr_getBands(int idx);
   int wlmngr_checkAfterburner(int idx);
   int wlmngr_checkWme(int idx);
   char wlmngr_scanForAddr(char *line, int isize, char **start, int *size);
   void wlmngr_aquireStationList(int idx);
   int wlmngr_getNumStations(int idx);
   void wlmngr_getStation(int idx, int i, char *macAddress, char *associated,  char *authorized, char *ssid, char *ifcName);
   int wlmngr_scanFileForMAC(char *fname, char *mac);
   void wlmngr_getCountryList(int idx,int argc, char **argv, char *list);
   void wlmngr_getChannelList(int idx,int argc, char **argv, char *list);
   void wlmngr_getVer(int idx );
   void wlmngr_getCurrentChannel(int idx);
   int wlmngr_getChannelImState(int idx);
   int wlmngr_getCurrentChSpec(int idx);
   int wlmngr_getMaxMbss(int idx);
   void wlmngr_setupMbssMacAddr(int idx);
   WL_STATUS wlmngr_enumInterfaces(int idx, char *devname);
   
#ifdef SUPPORT_MIMO   
   void wlmngr_getNPhyRates(int idx, int argc, char **argv, char *list);
#endif   
#ifdef WLCONF
   void wlmngr_doWlConf(int idx);
   void wlmngr_WlConfDown(int idx);
   void wlmngr_WlConfStart(int idx);
   void wlmngr_doWlconfAddi(int idx);
   void wlmngr_getFltMacList(int idx,int bssidx, char *buf, int size);
#else
   void wlmngr_doBasic(int idx);
   void wlmngr_doAdvanced(int idx);
   void wlmngr_doSecurityOne(int idx, int ssid_idx);
   void wlmngr_doMacFilter(int idx);
   void wlmngr_doWds(int idx);
#endif
   void wlmngr_doSecurity(int idx);
   void wlmngr_doWdsSec(int idx);
   void wlmngr_doQoS(int idx);   
   int  wlmngr_getValidChannel(int idx, int channel);
   long wlmngr_getValidRate(int idx, long rate);
   int  wlmngr_getCoreRev(int idx);
   char *wlmngr_getPhyType(int idx);
   void wlmngr_setSsid(int idx);
   int wlmngr_getValidBand(int idx, int band);   
   void wlmngr_getWdsMacList(int idx, char *buf, int size);
   void wlmngr_setWdsMacList(int idx, char *buf, int size);

#ifdef SUPPORT_WSC
   void wlmngr_genWscRandSssidPsk(int idx); //This function is used to generate Random SSID and PSK Key
   int wlmngr_wscPincheck(char *pin_string);
   int  wl_wscPinGet( void );
#endif

  void wlmngr_setupBridge(int idx);
   void wlmngr_stopBridge(int idx);
   void wlmngr_setupAntenna(int idx);
   void wlmngr_autoChannel(int idx);
   void wlmngr_setupRegulatory(int idx);     
   void wlmngr_printSsidList(int idx, char *text);
   void wlmngr_printMbssTbl(int idx, char *text);   
   bool wlmngr_getWdsWsec(int idx, int unit, int which, char *mac, char *role, char *crypto, char *auth, ...);
   void wlmngr_getWlInfo(int idx, char *buf, char *id);
   void wlmngr_getPwrSaveStatus(int idx, char *varValue);
   void wlmngr_togglePowerSave(void);
   void wlmngr_printScanResult(int idx, char *text);
   void wlmngr_saveApInfoFromScanResult(int apListIdx);
   bool wlmngr_isEnrSta(int idx);
   bool wlmngr_isEnrAP(int idx);
   void wlmngr_doWpsApScan(void);
   int wlmngr_ReadChipInfoStbc(void);
   int wlmngr_detectSMP(void);
   void wlmngr_restore_config_default(char *tftpip); 
   void wlmngr_write_nvram_default(char *);
#ifdef DMP_X_BROADCOM_COM_WIFIWAN_1
   void wlmngr_initUre(void);
   void wlmngr_initWanWifi(void);
#endif

/* Copy each token in wordlist delimited by space into word */
#define foreach(word, wordlist, next) \
	for (next = &wordlist[strspn(wordlist, " ")], \
	     strncpy(word, next, sizeof(word)), \
	     word[strcspn(word, " ")] = '\0', \
	     word[sizeof(word) - 1] = '\0', \
	     next = strchr(next, ' '); \
	     strlen(word); \
	     next = next ? &next[strspn(next, " ")] : (char*)"", \
	     strncpy(word, next, sizeof(word)), \
	     word[strcspn(word, " ")] = '\0', \
	     word[sizeof(word) - 1] = '\0', \
	     next = strchr(next, ' '))

//#define ARRAYSIZE(a)		(sizeof(a)/sizeof(a[0]))

#define bcmSystem(cmd)		bcmSystemEx (cmd,1)
#define bcmSystemMute(cmd)	bcmSystemEx (cmd,0)
char * wlmngr_get_nvramdefaultfile();
int prctl_getPidByName(const char *name);
#endif
