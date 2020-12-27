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
#include <string.h>
#include <sys/sysinfo.h>
#include <signal.h>

#include "device.h"
#include "war_type.h"
#include "war_string.h"
#include "tr_strings.h"
#include "log.h"
#include "ao.h"
#include "do.h"
#include "spv.h"
#include "inform.h"
#include "utils.h"
#include "cli.h"
#include "tr_strptime.h"
#include "xml.h"
#include "war_errorcode.h"

#ifdef ASUSWRT
#include <bcmnvram.h>
#include <notify_rc.h>
#else
#include "libtcapi.h"
#include <tcutils.h> 
#endif
#include <shared.h>
#include <shutils.h>
#include <queue.h>

 /* Asus specific */
CPU_OCCUPY cpu_stat1; 
CPU_OCCUPY cpu_stat2;

/* from shared */
#ifdef ASUSWRT
extern int check_imagefile(char *fname);
#endif

/* tr_lib.c function */
extern struct node *xml2tree( const char *file_path );
extern void free_tree( struct node *tree );
//extern int __tree2xml( struct node *tree, FILE *fp, int *level );
//extern const char *nocc_code2str( unsigned int code );

extern void reset_client_setting(int unit);
extern int read_ovpn_config_file(const char *file, int unit);

/* nvram helpers */
static int nvram_changed = 0;

#define VALUE_TYPE_ANY                  0x00
#define VALUE_TYPE_STRING               0x01
#define VALUE_TYPE_INT                  0x02
#define VALUE_TYPE_UNSIGNED_INT         0x03
#define VALUE_TYPE_BOOLEAN              0x04
#define VALUE_TYPE_DATE_TIME            0x05
#define VALUE_TYPE_BASE_64              0x06

#ifdef DSL_ASUSWRT
int dsl_pri, dsl_sec;
#endif
int wan_pri, wan_sec, lan_pri, lan_sec, usb_pri, usb_sec;
int pri_wan_inst = 0, sec_wan_inst = 0;
int support_dualwan = 0;
#ifdef RTCONFIG_XDSL
#if (defined(RTCONFIG_DSL) && defined(RTCONFIG_ADSL)) || (defined(TCSUPPORT_WAN_ATM) && defined(TCSUPPORT_WAN_PTM)) 
int support_adsl_vdsl = 1;
#else
int support_adsl_vdsl = 0;
#endif
#endif

#ifdef TR181
int mng_port_index = 0;		/* index for Device.Bridging management port */

#ifdef ASUSWRT
#define DEF_ETH_LAN_IFNAME	"vlan1"
#else 	/* DSL_ASUSWRT */
#define DEF_ETH_LAN_IFNAME	"eth0.1"
#endif
#endif

#ifdef RTCONFIG_XDSL
#define DSL_NUM					8
#ifdef TR098
#define PM_DEL					0
#define PM_ADD					1
#endif

#ifdef ASUSWRT
#define DSL_INFO_FILE			"/tmp/adsl/info_adsl.txt"
#ifdef RTCONFIG_DSL
#define ADSL_TRANS_MODE			"atm"
#endif
#ifdef RTCONFIG_VDSL
#define VDSL_TRANS_MODE			"ptm"
#endif

#else 	/* DSL_ASUSWRT */
#ifdef TCSUPPORT_WAN_ATM
#define ADSL_TRANS_MODE			"ADSL"
#endif
#ifdef TCSUPPORT_WAN_PTM
#define VDSL_TRANS_MODE			"VDSL"
#endif
#endif

#if defined(RTCONFIG_DSL) || defined(TCSUPPORT_WAN_ATM)
#define ADSL_ID 				1
#define ADSL_INTERNET_INDEX		0
#endif

#if defined(RTCONFIG_VDSL) || defined(TCSUPPORT_WAN_PTM)
#define VDSL_ID 				2
#define VDSL_INTERNET_INDEX		8
#endif

#endif	/* RTCONFIG_XDSL */


#define PROFILE_HEADER		"HDR1"
#ifdef RTCONFIG_DSL
#define PROFILE_HEADER_NEW	"N55U"
#else
#define PROFILE_HEADER_NEW	"HDR2"
#endif


/* device functions */
struct notify_item {
	TAILQ_ENTRY(notify_item) entry;
	char service[32];
};
TAILQ_HEAD(head, notify_item) notify_queue;

int dev_init(char *arg)
{
	update_flag = 1;

	TAILQ_INIT(&notify_queue);

	prepare_wan_info();		/* prepare wan info if need */

	get_cpuoccupy((CPU_OCCUPY * )&cpu_stat1);

#ifdef TR098	/* start of TR098 */
#ifdef ASUSWRT
	sw_mode = sw_mode();
#else
	sw_mode = SW_MODE_ROUTER;
#endif
	update_device_summary();		/* update device summary */
    update_deviceInfo_description();/* set the value of InternetGatewayDevice.deviceInfo.description */
	update_wlan_device();       	/* update wlandevice */
	if (sw_mode == SW_MODE_ROUTER) {
		update_manageable_device();		/* update manageable device */
		update_dhcp_static_addr();		/* update static address for dhcp */
		update_wan_device();			/* update wandevice */
		update_port_mapping();		/* update port mapping for wandevice */	
		update_forwarding();			/* update l3 forwarding */
	}
#endif	/* end of TR098 */

#ifdef TR181	/* start of TR181 */
#ifdef ASUSWRT	
	/**
	 * 1:SW_MODE_ROUTER
	 * 2:SW_MODE_REPEATER
	 * 3:SW_MODE_AP
	 * 4:SW_MODE_HOTSPOT // sw_mode=3 and nvram_get("wlc_psta")=1
	 */
	sw_mode = sw_mode();
	tr_mode = nvram_get_int(TR_MODE);	/* last operation mode */
	if(!tr_mode) {
		tr_mode = sw_mode;
		nvram_set_int(TR_MODE, sw_mode);
	}


	update_processor_info();		/* update processor info */
	update_gateway_info();		/* update gateway info */
#if defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK) //sherry 2016.2.19  AP mode
	if(sw_mode == SW_MODE_ROUTER || sw_mode == SW_MODE_AP)
#else
	if(sw_mode == SW_MODE_ROUTER || (sw_mode == SW_MODE_AP && nvram_get_int("wlc_psta") != 1)) //Router and AP mode
#endif
	{
		//update_gateway_info();		/* update gateway info */
		update_wan_lan_if();            /* update wan and lan */
		update_wifi_radio_ssid_ap();	/* update wifi (radio, ssid, ap) */
		update_bridging();				/* update bridging */
		update_dhcpv4_client();		/* update dhcpv4 client */
	}
#if defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK) //sherry 2016.2.19  Repeater mode
	if((sw_mode == SW_MODE_REPEATER) && (nvram_get_int("wlc_psta") != 1) )
#else
	if(sw_mode == SW_MODE_REPEATER) //Repeater mode
#endif
	{
		update_wifi_radio_ssid_ap();	/* update wifi (radio, ssid, ap) */
		update_bridging();				/* update bridging */
	}

	if(sw_mode == SW_MODE_ROUTER) //Router mode
	{
		update_port_mapping();		/* update port mapping for wandevice */
		update_forwarding();		/* update ipv4 forwarding */
		update_host();				/* update host */
		update_dhcp_static_addr();	/* update static address for dhcp */
	}
	nvram_set_int(TR_MODE, sw_mode);

#else 	/* DSL_ASUSWRT */
	sw_mode = SW_MODE_ROUTER;
	tr_mode = SW_MODE_ROUTER;
	update_processor_info();		/* update processor info */
	update_gateway_info();			/* update gateway info */
	update_wan_lan_if();            /* update wan and lan */
	update_wifi_radio_ssid_ap();	/* update wifi (radio, ssid, ap) */
	update_bridging();				/* update bridging */
	update_dhcpv4_client();		/* update dhcpv4 client */
	update_port_mapping();		/* update port mapping for wandevice */
	update_forwarding();		/* update ipv4 forwarding */
	update_host();				/* update host */
	update_dhcp_static_addr();	/* update static address for dhcp */
#endif
#endif	/* end of TR181 */

#ifdef RTCONFIG_SFEXPRESS	
	update_ovpnc();					/* update openvpn client */
#endif

	update_node_value();			/* update the value of all nodes */
	update_new_xml();				/* update new xml file from xml.bak file */

#ifdef TR232
	update_profile_sched();			/* update scheduler for profile of bulkdata */
#endif

	update_flag = 0;

	return 0;
}

int dev_notify_queued(void)
{
	struct notify_item *item, *next_item;

	TAILQ_FOREACH_SAFE(item, &notify_queue, entry, next_item) {
		TAILQ_REMOVE(&notify_queue, item, entry);
		if (*item->service)
#ifdef ASUSWRT
			notify_rc(item->service);
#else 	/* DSL_ASUSWRT */
			tcapi_commit(item->service);
#endif
		free(item);
	}
	return 0;
}

int dev_notify(char *services, char *arg, int queue)
{

	struct notify_item *item;
	char *service, *first, *next;
	char buf[64] = {0};

	/* unqueued notify, process queue first */
	if (!queue)
		dev_notify_queued();

	if (!services || *services == '\0')
		return 0;

	tr_log(LOG_DEBUG, "%s - service (%s), arg (%s)", __FUNCTION__, services, arg);

	/* username or password is changed, so we need call rc cmd : chpass, restart_ftpsamba(ifdef RTCONFIG_USB), restart_time */
//#ifdef TR181
	if(strcmp(services, "restart_user") == 0) {
#ifdef RTCONFIG_USB
		snprintf(buf, sizeof(buf), "%s", "chpass; restart_ftpsamba; restart_time");
#endif
		snprintf(buf, sizeof(buf), "%s", "chpass; restart_time");
	}
//#endif

	first = next = strdup(services);
	while ((service = strsep(&next, "; "))) {
		if (*service == '\0')
			continue;

		/* place arg exceptions here */
#ifdef ASUSWRT
		if (strcmp(service, "restart_wan_if") == 0) {
#ifdef TR098
			int unit = get_wan_unit_by_path(arg);
#endif
#ifdef TR181
			char prefix[16] = {0}, tmp[32] = {0};
			int unit;

			if (sw_mode == SW_MODE_ROUTER) {			
				if(strncmp(arg, CLIENT_DHCP_1, strlen(CLIENT_DHCP_1)) == 0)
					unit = 0;
				else if(strncmp(arg, CLIENT_DHCP_2, strlen(CLIENT_DHCP_2)) == 0)
					unit = 1;
				else
					unit = ethernet_unit_by_path(arg);
				/* get prefix */
				sprintf(prefix, "%s", ethernet_prefix_by_path(arg, tmp));
			}
			else if (sw_mode == SW_MODE_AP)
				unit = 2;
#endif

			/* check ASUS specific */
			if (
#ifdef RTCONFIG_XDSL
				strstr(arg, "X_ASUS_DslType") ||
#else
				strstr(arg, "X_ASUS_WanType") ||
#endif
				strstr(arg, "X_ASUS_LanType") || strstr(arg, "X_ASUS_UsbType")) {
				node_t node_arg;

				lib_resolve_node(arg, &node_arg);
				unit = get_wan_prefix_ifunit(node_arg);
			}

#ifdef TR181
#ifdef RTCONFIG_XDSL
			if (pri_wan_inst == DEVICE_DSL_INST && !strcmp(prefix, "wan0_")) 		/* replace restart_wan_if with restart_dslwan_if for dsl */
				snprintf(buf, sizeof(buf), "restart_dslwan_if %d", WAN_UNIT_FIRST);
			else if (sec_wan_inst == DEVICE_DSL_INST && !strcmp(prefix, "wan1_"))	/* replace restart_wan_if with restart_dslwan_if for dsl */
				snprintf(buf, sizeof(buf), "restart_dslwan_if %d", WAN_UNIT_SECOND);
			else
#endif
#endif
			if(unit == 2)
				snprintf(buf, sizeof(buf), "%s", "restart_net_and_phy");
			else
				snprintf(buf, sizeof(buf), "%s %d", service, unit);
			//service = buf;
		}


#ifdef RTCONFIG_XDSL
		if (!strcmp(service, "restart_dslwan_if")) {
			if (pri_wan_inst == 
#ifdef TR098
				IGD_WANDEVICE_DSL_INST
#endif
#ifdef TR181
				DEVICE_DSL_INST
#endif
				)
				snprintf(buf, sizeof(buf), "%s %d", service, WAN_UNIT_FIRST);
			else if (sec_wan_inst == 
#ifdef TR098
				IGD_WANDEVICE_DSL_INST
#endif
#ifdef TR181
				DEVICE_DSL_INST
#endif
				)
				snprintf(buf, sizeof(buf), "%s %d", service, WAN_UNIT_SECOND);

			if (nvram_match("dslx_transmode", ADSL_TRANS_MODE)) {
#ifdef TR098
				if (strstr(arg, "WANDSLLinkConfig")) {
					if (strstr(arg, "WANDSLLinkConfig.Enable")) {
#endif
#ifdef TR181
				if (strstr(arg, ATM_LINK)) {
					if (strstr(arg, "Enable")) {
#endif		
						char enable[8] = {0};

						__get_parameter_value(arg, enable);
						if (!strcasecmp(enable, "true") || !strcmp(enable, "1"))
							snprintf(buf, sizeof(buf), "reboot");
					}
					else
					{
						char path_buf[128] = {0}, dsl_prefix[16] = {0};
#ifdef TR098
						int wd_inst = getWANDevInstNum(arg);
						int wcd_inst = getWANConDevInstNum(arg);

						snprintf(path_buf, sizeof(path_buf), "%s.%d.WANConnectionDevice.%d.WANDSLLinkConfig.X_ASUS_DSLIndex", IGD_WANDEVICE, wd_inst, wcd_inst);
#endif
#ifdef TR181
						unsigned#ifdef TR098	/* start of TR098 */ int link_inst = getInstNum(arg, "Link");
						snprintf(path_buf, sizeof(path_buf), "%s.%d.X_ASUS_DSLIndex", ATM_LINK, link_inst);
#endif					
						memset(dsl_prefix, 0, sizeof(dsl_prefix));
						__get_parameter_value(path_buf, dsl_prefix);

						if (strcmp(dsl_prefix, "dsl0")) /* dsl1~dsl7 need to be replaced reboot by restart_dslwan_if */
							snprintf(buf, sizeof(buf), "reboot");
					}
				}
			}
#ifdef RTCONFIG_VDSL
			else if (nvram_match("dslx_transmode", VDSL_TRANS_MODE)) {
#ifdef TR098
				if (strstr(arg, "WANDSLLinkConfig")) {
					if (strstr(arg, "WANDSLLinkConfig.Enable")) {
#endif
#ifdef TR181
				if (strstr(arg, PTM_LINK)) {
					if (strstr(arg, "Enable")) {
#endif						
						char enable[8] = {0};

						__get_parameter_value(arg, enable);
						if (!strcasecmp(enable, "true") || !strcmp(enable, "1"))
							snprintf(buf, sizeof(buf), "reboot");
					}
				}
			}
#endif
			//service = buf;
		}
#endif


#else 	/* DSL_ASUSWRT */
		int add_adsl_action = 0;

		if (strcmp(service, "Wan_PVC") == 0) {
			int unit = 0;
#ifdef TR098
			unit = get_wan_unit_by_path(arg);
#endif
#ifdef TR181
			//char prefix[16] = {0}, tmp[32] = {0};
			//int unit;
			
			if(strncmp(arg, CLIENT_DHCP_1, strlen(CLIENT_DHCP_1)) == 0) {
				if (dsl_pri) {
#ifdef TCSUPPORT_WAN_ATM
					unit = WAN_ATM_INDEX;
#endif
#ifdef TCSUPPORT_WAN_PTM
					unit = WAN_PTM_INDEX;
#endif
				}
#ifdef TCSUPPORT_WAN_ETHER			
				else if (wan_pri)
					unit = WAN_ETHER_INDEX;
#endif
#ifdef RTCONFIG_USB_MODEM
				else if (usb_pri)
					unit = WAN_USB_INDEX;
#endif
			}
			else if(strncmp(arg, CLIENT_DHCP_2, strlen(CLIENT_DHCP_2)) == 0)
			{
				if (dsl_sec) {
#ifdef TCSUPPORT_WAN_ATM
					unit = WAN_ATM_INDEX;
#endif
#ifdef TCSUPPORT_WAN_PTM
					unit = WAN_PTM_INDEX;
#endif
				}
#ifdef TCSUPPORT_WAN_ETHER			
				else if (wan_sec)
					unit = WAN_ETHER_INDEX;
#endif
#ifdef RTCONFIG_USB_MODEM
				else if (usb_sec)
					unit = WAN_USB_INDEX;
#endif
			}
			else
				unit = ethernet_unit_by_path(arg);
			/* get prefix */
			//sprintf(prefix, "%s", ethernet_prefix_by_path(arg, tmp));
#endif

			/* check ASUS specific */
			if (strstr(arg, "X_ASUS_DslType")
#ifdef TCSUPPORT_WAN_ETHER
				|| strstr(arg, "X_ASUS_WanType")
#endif
#ifdef RTCONFIG_USB_MODEM
				|| strstr(arg, "X_ASUS_UsbType")
#endif
				) {
				node_t node_arg;

				lib_resolve_node(arg, &node_arg);
				unit = get_wan_prefix_ifunit(node_arg);
			}

#ifdef TR181
#ifdef RTCONFIG_XDSL

			if (
#if defined(TCSUPPORT_WAN_ATM) && defined(TCSUPPORT_WAN_PTM)
				unit == WAN_ATM_INDEX || unit == WAN_PTM_INDEX
#else
#ifdef TCSUPPORT_WAN_ATM
				unit == WAN_ATM_INDEX
#endif
#ifdef TCSUPPORT_WAN_PTM
				unit == WAN_PTM_INDEX
#endif
			)
#endif
			
			{
				if (pri_wan_inst == DEVICE_DSL_INST || sec_wan_inst == DEVICE_DSL_INST)	/* add Adsl_Entry action for dsl */
					add_adsl_action = 1;
			}
#endif
#endif

			snprintf(buf, sizeof(buf), "%s%d", service, unit);
			//service = buf;
		}
#endif

#ifdef RTCONFIG_SFEXPRESS
		if (!strcmp(service, "restart_vpnclient") || !strcmp(service, "start_vpnclient") ||
		 !strcmp(service, "stop_vpnclient")) {
			node_t node_arg;
			int index_t = 0;

			lib_resolve_node(arg, &node_arg);
			index_t = get_ovpnc_index_value(node_arg);

			if (index_t) {
				snprintf(buf, sizeof(buf), "%s%d", service, index_t);
				//service = buf;
			}
		}
#endif	//RTCONFIG_SFEXPRESS


		/* replace service with buf */
		if (strlen(buf))
			service = buf;


		/* notify or add to the queue, if requested */
		if (queue) {
			/* search for dups */
			TAILQ_FOREACH(item, &notify_queue, entry) {
				if (strncmp(item->service, service, sizeof(item->service)) == 0) {
					service = NULL;
					break;
				}
			}
			/* add to queue */
			if (service && (item = malloc(sizeof(*item)))) {
				snprintf(item->service, sizeof(item->service), "%s", service);
				TAILQ_INSERT_TAIL(&notify_queue, item, entry);
			}

#ifdef DSL_ASUSWRT	/* add Adsl_Entry for dsl */
			if (add_adsl_action) {
				snprintf(buf, sizeof(buf), "Adsl_Entry");
				service = buf;

				/* search for dups */
				TAILQ_FOREACH(item, &notify_queue, entry) {
					if (strncmp(item->service, service, sizeof(item->service)) == 0) {
						service = NULL;
						break;
					}
				}
				/* add to queue */
				if (service && (item = malloc(sizeof(*item)))) {
					snprintf(item->service, sizeof(item->service), "%s", service);
					TAILQ_INSERT_TAIL(&notify_queue, item, entry);
				}
			}
#endif
		} else {
			/* unqueued notify */
#ifdef ASUSWRT
			notify_rc(service);
#else
			tcapi_commit(service);
#endif
		}
	}
	free(first);

	return 0;
}

void dev_reboot(void)
{
#ifdef ASUSWRT
	notify_rc("reboot");
#else 	/* DSL_ASUSWRT */
	system("reboot");
#endif
}

/* firmware functions */
int dev_firmware_upgrade(char *path)
{
//_cassie_ modify
	int ret = -1;
    int ret_check_image= -1;
#ifdef ASUSWRT
	//if (check_imagefile(path)) {
	ret_check_image = check_imagefile(path);
	if(ret_check_image == 0){
		if (nvram_contains_word("rc_support", "nandflash"))	/* RT-AC56U/RT-AC68U/RT-N16UHP */
		{	 
			tr_log( LOG_ERROR, "-------dev_firmware_upgrade-------\n");
			//eval("mtd-write2", path, "linux");
			system("mv  /tmp/firmware.trx  /tmp/linux.trx");
            notify_rc("start_upgrade");
		}
		else{
			//TODO 
			//eval("mtd-write", "-i", path, "-d", "linux");
		}
		sleep(10);
		ret = 1; /* 0: ok, 1 reboot required */
	}
	//unlink(path);
#else 	/* DSL_ASUSWRT */
	char upgrade_fw_status[16] = {0};

	tcapi_set("System_Entry", "tr_upgrade_fw", "1");
	tcapi_set("System_Entry", "upgrade_fw", "1");
	tcapi_commit("System_Entry");

	/* check the status of firmware upgrade */
	while (1) {
		sleep(5);	/* wait for the status of firmware upgrade */
		ret = tcapi_get("System_Entry", "upgrade_fw_status", upgrade_fw_status);
		if (ret == 0) {
			if (strcmp(upgrade_fw_status, "NONE")) {
				if (!strcmp(upgrade_fw_status, "SUCCESS"))
					ret = 1;
				break;
			}
		}
	}
#endif	
	return ret;	/* 0: ok, 1 reboot required */
}

int dev_settings_commit(void)
{
	if (nvram_changed)
#ifdef ASUSWRT
		nvram_commit();
#else 	/* DSL_ASUSWRT */
		tcapi_save();
#endif
	nvram_changed = 0;

	return 0;
}

int modprobe_r(const char *mod)
{
#if 1
	return eval("modprobe", "-r", (char *)mod);
#else
	int r = eval("modprobe", "-r", (char *)mod);
	cprintf("modprobe -r %s = %d\n", mod, r);
	return r;
#endif
}

int g_reboot = 0;

int dev_settings_reset(void)
{
#ifdef RTCONFIG_CONCURRENTREPEATER
	nvram_set_int("led_status", LED_FACTORY_RESET);
#endif
	g_reboot = 1;
	f_write_string("/tmp/reboot", "1", 0, 0);
#ifdef RTCONFIG_REALTEK
/* [MUST] : Need to Clarify ... */
	set_led(LED_BLINK_SLOW, LED_BLINK_SLOW);
	nvram_commit();
#endif
#ifdef RTCONFIG_DSL
	eval("adslate", "sysdefault");
#endif
	eval("service", "stop_wan");

#ifdef RTCONFIG_NOTIFICATION_CENTER
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	eval("rm", "-rf", NOTIFY_DB_FOLDER);
#endif
#endif

#ifdef RTCONFIG_USB
#ifdef RTCONFIG_USB_MODEM
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	eval("rm", "-rf", "/jffs/sim");
#endif
#endif
#ifdef RTCONFIG_OPENVPN
#if defined(RTCONFIG_UBIFS)
	eval("rm", "-rf", OVPN_DIR_SAVE);
#endif
#endif

	if(get_model() == MODEL_RTN53)
	{
		eval("wlconf", "eth2", "down");
		modprobe_r("wl_high");
	}

#if !defined(RTN56UB1) && !defined(RTN56UB2)
#ifndef RTCONFIG_ERPTEST
	eval("service", "stop_usb");
#else
	stop_usb(0);
#endif
#ifndef RTCONFIG_NO_USBPORT
	eval("service", "stop_usbled");
#endif
#endif
#endif

	sleep(3);
	nvram_set(ASUS_STOP_COMMIT, "1");
#if defined(RTCONFIG_NVRAM_FILE)
	ResetDefault();
#else
	if(nvram_contains_word("rc_support", "nandflash"))	/* RT-AC56S,U/RT-AC68U/RT-N18U */
	{
#if defined(RTCONFIG_ALPINE) || defined(RTCONFIG_LANTIQ)
		system("mtd-erase -d nvram");
#else
#ifdef HND_ROUTER
		eval("hnd-erase", "nvram");
#else
		eval("mtd-erase2", "nvram");
#endif
#endif
	}
	else
	{
#if defined(RTAC1200G) || defined(RTAC1200GP)
		eval("mtd-erase2", "nvram");
#elif defined(RTCONFIG_REALTEK)
		ResetDefault();
#else
		eval("mtd-erase", "-d", "nvram");
#endif
#endif
	}
#ifdef RTCONFIG_QCA_PLC_UTILS
	reset_plc();
	eval("mtd-erase", "-d", "plc");
#endif

	kill(1, SIGTERM);
}

int dev_settings_save(char *path)
{
#ifdef ASUSWRT
	eval("nvram", "save", path);
#else 	/* DSL_ASUSWRT */
	handleRomfile();
#endif
	return 0;
}

int dev_settings_restore(char *path)
{
#ifdef ASUSWRT
	eval("nvram", "restore", path);
	nvram_commit();
	unlink(path);
	return 1; /* 0: ok, 1 reboot required */

#else 	/* DSL_ASUSWRT */
	int ret = -1;
	char cfg_status[16] = {0};

	tcapi_set("System_Entry", "tr_upgrade_fw", "1");
	tcapi_set("System_Entry", "upgrade_fw", "1");
	tcapi_commit("System_Entry");

	/* check the status of cfg upgrade */
	while (1) {
		sleep(5);	/* wait for the status of cfg upgrade */
		ret = tcapi_get("System_Entry", "upgrade_fw_status", cfg_status);
		if (ret == 0) {
			if (strcmp(cfg_status, "NONE")) {
				if (!strcmp(cfg_status, "SUCCESS"))
					ret = 1;
				break;
			}
		}
	}
	return ret;	/* 0: ok, 1 reboot required */
#endif
}

/* compare *.xml and *.xml.bak version
return value:: 0 same
	       1 differ 
	       2 have no *.xml.bak*/
int check_xml_version( const char *arg )
{
	char bak[256] = {0}, buf[256] = {0}, version[10] = {0}, version_bak[10] = {0};
	FILE *fp;
	char *ptr = NULL;
	int i = 0;

	snprintf(bak, sizeof(bak), "%s.bak", arg);
	memset(version, 0, sizeof(version));
	memset(version_bak, 0, sizeof(version));

	fp = tr_fopen(arg, "r");

	if( fp ) {
		while(fgets(buf, sizeof(buf), fp) != NULL) {
			if((ptr = strstr(buf, "Device")) != NULL) {
				if((ptr = strstr(buf, "arg=")) != NULL) {
					ptr = ptr + 5;
					i = 0;
					while(*ptr != '\'') {
						version[i] = *ptr;
						ptr++;
						i++;
					}
					version[i] = '\0';
					break;
				}
			}
			else
				continue;
		}
		fclose(fp);
	}
	else
		return -1;

	fp = tr_fopen( bak, "r" );

	if( fp ) {
		while(fgets(buf, sizeof(buf), fp) != NULL) {
			if((ptr = strstr(buf, "Device")) != NULL) {
				if((ptr = strstr(buf, "arg=")) != NULL) {
					ptr = ptr + 5;
					i = 0;
					while(*ptr != '\'') {
						version_bak[i] = *ptr;
						ptr++;
						i++;
					}
					version_bak[i] = '\0';
					break;
				}
			}
			else
				continue;
		}
		fclose(fp);
	}
	else
		return 2;
	
	tr_log(LOG_DEBUG, "ver::%s ver_bak::%s", version, version_bak);

	//return strcmp(version, version_bak);
	if(strcmp(version, version_bak) == 0)
		return 0;
	else
		return 1;
}

void __save_old_setting( struct node *tree, FILE *fp )
{
	if (strcmp(tree->type, "node") == 0) {
		struct node *n;
		for(n = tree->children; n; n = n->brother) {
			__save_old_setting(n , fp);
		}
	}
	else {
		char *v = xml_str2xmlstr(tree->value);
		char *path = lib_node2path(tree);
		/*if(strstr( path, "template" ) || (tree->dev.cmd != NULL && tree->noc == 0 && tree->nocc == 0)) {
			lib_destroy_tree( tree );//cannot free unwanted node
			tree = NULL;
		}*/
		if (tree->noc != 0 || strlen(tree->acl) != 0){
			if (strlen(tree->acl) != 0)
				fprintf(fp, "%s %d %s %s\n", path, tree->noc, tree->acl, v ? v : tree->value);
			else
				fprintf(fp, "%s %d %s %s\n", path, tree->noc, "Space", v ? v : tree->value);
			if (v)
 				free(v);
		}
	}
}

int save_old_xml(char *arg)
{
	char bakfile[strlen(arg) + 5];
	FILE *fp;

	if (check_xml_version(arg) != 1)
		return 1;

	snprintf(bakfile, sizeof(bakfile), "%s.bak", arg);
	struct node *rootbak = xml2tree( bakfile );

	fp = fopen("/tmp/tr_bak", "w");

	if (rootbak != NULL && fp != NULL) {
		__save_old_setting(rootbak, fp);
		free_tree( rootbak );
		rootbak = NULL;
	}
	else
		return -1;

	fflush(fp);
	fclose(fp);

	return 0;
}

int update_new_xml()
{
	FILE *fp;
	char buf[256] = {0};

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	if (access("/tmp/tr_bak", F_OK) != 0)
		return 0;

	fp = fopen("/tmp/tr_bak", "r");

	if (fp == NULL)
		return -1;

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		char path[128] = {0}, acl[8] = {0}, value[128] = {0};
		int noc = 0;

		sscanf(buf, "%s %d %s %s", path, &noc, acl, value);
		compare_bak_xml(path, noc, acl, value);
	}

	fclose(fp);
	unlink("/tmp/tr_bak");

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

/* nvram functions */
#ifdef DSL_ASUSWRT
static char *query_node_name(char *name, char *result)
{
	if (!strcmp(name, "ProductName"))
		sprintf(result, "%s", SYSINFO_NODE);
	else if (!strcmp(name, "pvgcode") || !strcmp(name, "tr_enable") ||
		!strcmp(name, "tr_acs_url") || !strcmp(name, "tr_username") || !strcmp(name, "tr_passwd") || 
		!strcmp(name, "tr_inform_enable") || !strcmp(name, "tr_inform_interval") || !strcmp(name, "tr_conn_username") ||
		!strcmp(name, "tr_conn_passwd"))
		sprintf(result, "%s", TR069_NODE);
#ifdef TR098
	else if (!strcmp(name, "IP") || !strcmp(name, "netmask"))
		sprintf(result, "%s", LAN_NODE);
#endif
	else if (!strcmp(name, "start") || !strcmp(name, "end") || !strcmp(name, "router") || !strcmp(name, "lease"))
		sprintf(result, "%s", DHCPD_COMMON_NODE);
	else if (!strcmp(name, "SERVER"))
		sprintf(result, "%s", TIMEZONE_NODE);
	else if (!strcmp(name, "modem_enable") || !strcmp(name, "modem_country") || !strcmp(name, "modem_isp") ||
			!strcmp(name, "modem_apn") || !strcmp(name, "modem_dialnum") || !strcmp(name, "modem_pincode") ||
			!strcmp(name, "modem_user") || !strcmp(name, "modem_pass") || !strcmp(name, "modem_pass"))
		sprintf(result, "%s", USBMODEM_NODE);
	else if (!strcmp(name, "wandog_interval") || !strcmp(name, "wandog_delay") || !strcmp(name, "wandog_maxfail") ||
			!strcmp(name, "wandog_enable") || !strcmp(name, "wandog_target") || !strcmp(name, "wans_mode"))
		sprintf(result, "%s", DUALWAN_NODE);
#if defined(TCSUPPORT_WAN_ATM) && defined(TCSUPPORT_WAN_PTM)
	else if (!strcmp(name, "DSLMode"))
		sprintf(result, "%s", WAN_COMMON_NODE);
#endif
#ifdef TR181
	else if (!strcmp(name, "misc_http_x") || !strcmp(name, "fw_enable_x"))
		sprintf(result, "%s", FIREWALL_NODE);
	else if (!strcmp(name, "BRIPv4Addr") || !strcmp(name, "Prefix") || !strcmp(name, "IPv4MaskLen"))
		sprintf(result, "%s", IPV6RD_NODE);
#endif


	return result;
}
#endif 	/* DSL_ASUSWRT */

static char *nvram_get_type(char *name, int type)
{
#ifdef ASUSWRT
	char *value = nvram_safe_get(name);
#else 	/* DSL_ASUSWRT */
	char buf[128] = {0}, node_name[32] = {0};
	char *value = NULL;
	
	memset(node_name, 0, sizeof(node_name));
	query_node_name(name, node_name);

	if (strlen(node_name) == 0)
		return "";

	value = tcapi_get_string(node_name, name, buf);
#endif

	switch (type) {
	case VALUE_TYPE_BOOLEAN:
		return atoi(value) ? "true" : "false";
	case VALUE_TYPE_INT:
	case VALUE_TYPE_UNSIGNED_INT:
		return atoi(value) ? value : "0";
	}

	return value;
}

static int get_nvram_value(node_t node, char *arg, char **value)
{
	int type = VALUE_TYPE_STRING;

	if (!arg || *arg == '\0')
		return -1;

	if (node) {
		char ptype[PROPERTY_LENGTH];
		if (lib_get_property(node, "type", ptype) == 0) {
			if (strcasecmp(ptype, "string") == 0)
				type = VALUE_TYPE_STRING;
			else if (strcasecmp(ptype, "boolean") == 0)
				type = VALUE_TYPE_BOOLEAN;
			else if (strcasecmp(ptype, "int") == 0)
				type = VALUE_TYPE_INT;
			else if (strcasecmp(ptype, "unsignedInt") == 0)
				type = VALUE_TYPE_UNSIGNED_INT;
		}
	}
	*value = nvram_get_type(arg, type);

	*value = strdup(*value);
	return *value ? 0 : -1;
}

static int nvram_set_type(char *name, char *value, int type)
{
	char buf[sizeof("-4294967295")] = {0};
	int res;

#ifdef DSL_ASUSWRT
	char node_name[32] = {0};
	
	memset(node_name, 0, sizeof(node_name));
	query_node_name(name, node_name);

	if (strlen(node_name) == 0)
		return -1;
#endif

	switch (type) {
	case VALUE_TYPE_BOOLEAN:
		res = string2boolean(value);
		/* if (res == BOOLEAN_ERROR)
			return -1; */
#ifdef ASUSWRT
		res = nvram_set_int(name, (res == BOOLEAN_TRUE) ? 1 : 0);
#else 	/* DSL_ASUSWRT */
		res = tcapi_set(node_name, name, (res == BOOLEAN_TRUE) ? "1" : "0");
#endif
		break;
	case VALUE_TYPE_INT:
#ifdef ASUSWRT
		res = nvram_set_int(name, atoi(value));
#else 	/* DSL_ASUSWRT */
		res = tcapi_set(node_name, name, value);
#endif
		break;
	case VALUE_TYPE_UNSIGNED_INT:
		snprintf(buf, sizeof(buf), "%lu", strtoul(value, NULL, 10));
#ifdef ASUSWRT
		res = nvram_set(name, buf);
#else 	/* DSL_ASUSWRT */
		res = tcapi_set(node_name, name, buf);
#endif
		break;
	default:
#ifdef ASUSWRT
		res = nvram_set(name, value);
#else 	/* DSL_ASUSWRT */
		res = tcapi_set(node_name, name, value);
#endif
	}

	nvram_changed = 1;
	return res;
}

static int set_nvram_value(node_t node, char *arg, char *value)
{
	int type = VALUE_TYPE_STRING;

	if (!arg || *arg == '\0')
		return -1;

	if (node) {
		char ptype[PROPERTY_LENGTH];
		if (lib_get_property(node, "type", ptype) == 0) {
			if (strcasecmp(ptype, "string") == 0)
				type = VALUE_TYPE_STRING;
			else if (strcasecmp(ptype, "boolean") == 0)
				type = VALUE_TYPE_BOOLEAN;
			else if (strcasecmp(ptype, "int") == 0)
				type = VALUE_TYPE_INT;
			else if (strcasecmp(ptype, "unsignedInt") == 0)
			type = VALUE_TYPE_UNSIGNED_INT;
		}
	}
	return nvram_set_type(arg, value, type);
}
#ifdef ASUSWRT
#define set_nvram(arg, value) set_nvram_value(NULL, arg, value)
#define get_nvram(arg, value) get_nvram_value(NULL, arg, value)

#else 	/* DSL_ASUSWRT */
static int set_attr_value(char *node_name, char *attr_name, char *value)
{
	int res;

	res = tcapi_set(node_name, attr_name, value);

	if (res != 0)
		return -1;

	nvram_changed = 1;
	return res;
}

static int get_attr_value(char *node_name, char *attr_name, char **value)
{
	int res;
	char buf[64] = {0};

	res = tcapi_get(node_name, attr_name, buf);

	if (res != 0)
		return -1;

	*value = buf;

	*value = strdup(*value);
	return *value ? 0 : -1;
}
#define set_nvram(node, attr, value) set_attr_value(node, attr, value)
#define get_nvram(node, attr, value) get_attr_value(node, attr, value)
#endif

/* cmd functions */
void prepare_wan_info(void)
{
#ifdef ASUSWRT
	char *rc_support = nvram_safe_get("rc_support");
	char *wans_dualwan = nvram_safe_get("wans_dualwan");
#else 	/* DSL_ASUSWRT */
	char rc_support[256] = {0};
	char wans_dualwan[32] = {0};

	tcapi_get(SYSINFO_NODE, "rc_support", rc_support);
	tcapi_get(DUALWAN_NODE, "wans_dualwan", wans_dualwan);
#endif

	if (find_word(rc_support, "dualwan"))
		support_dualwan = 1;

	if (support_dualwan) {
#ifdef ASUSWRT
#ifdef RTCONFIG_XDSL
		/* check dsl is primary or second wan */
		if (!strncmp(wans_dualwan, "dsl", 3))
			wan_pri = 1;
		else if (strstr(wans_dualwan, "dsl"))
			wan_sec = 1;
#else
		/* check wan is primary or second wan */
		if (!strncmp(wans_dualwan, "wan", 3))
			wan_pri = 1;
		else if (strstr(wans_dualwan, "wan"))
			wan_sec = 1;
#endif

#else 	/* DSL_ASUSWRT */
		if (!strncmp(wans_dualwan, "dsl", 3))
			dsl_pri = 1;
		else if (strstr(wans_dualwan, "dsl"))
			dsl_sec = 1;

		if (!strncmp(wans_dualwan, "wan", 3))
			wan_pri = 1;
		else if (strstr(wans_dualwan, "wan"))
			wan_sec = 1;				
#endif	//ASUSWRT

		/* check lan is primary or second wan */
		if (!strncmp(wans_dualwan, "lan", 3))
			lan_pri = 1;
		else if (strstr(wans_dualwan, "lan"))
			lan_sec = 1;

		/* check usb is primary or second wan */
		if (!strncmp(wans_dualwan, "usb", 3))
			usb_pri = 1;
		else if (strstr(wans_dualwan, "usb"))
			usb_sec = 1;

		/* process primary wan */
#ifdef DSL_ASUSWRT
		if (dsl_pri) {
#ifdef TR098
			pri_wan_inst = IGD_WANDEVICE_DSL_INST;		
#endif
#ifdef TR181
			pri_wan_inst = DEVICE_DSL_INST;
#endif
			if (wan_sec)
#ifdef TR098
				sec_wan_inst = IGD_WANDEVICE_WAN_INST;
#endif
#ifdef TR181
				sec_wan_inst = DEVICE_ETH_WAN_INST;
#endif				
			else if (lan_sec)
#ifdef TR098
				sec_wan_inst = IGD_WANDEVICE_LAN_INST;
#endif
#ifdef TR181
#if defined(ASUSWRT) && defined(RTCONFIG_DUALWAN)
				sec_wan_inst = DEVICE_ETH_WANLAN_INST;
#else
				; /* do nothing */
#endif
#endif
			else if (usb_sec)
#ifdef TR098
				sec_wan_inst = IGD_WANDEVICE_USB_INST;
#endif
#ifdef TR181
				sec_wan_inst = DEVICE_USB_INST;
#endif
			else
				tr_log(LOG_DEBUG, "%s - secondary wan is none", __FUNCTION__);			
		}
		else

#endif 	/* DSL_ASUSWRT */
		if (wan_pri) {
#ifdef TR098			
			pri_wan_inst = IGD_WANDEVICE_WAN_INST;		
#endif
#ifdef TR181
#ifdef ASUSWRT
#ifdef RTCONFIG_XDSL
			pri_wan_inst = DEVICE_DSL_INST;
#else			
			pri_wan_inst = DEVICE_ETH_WAN_INST;
#endif
#else 	/* DSL_ASUSWRT */
			pri_wan_inst = DEVICE_ETH_WAN_INST;
#endif
#endif	/* TR181 */
			if (lan_sec)
#ifdef TR098
				sec_wan_inst = IGD_WANDEVICE_LAN_INST;
#endif
#ifdef TR181
#if defined(ASUSWRT) && defined(RTCONFIG_DUALWAN)
				sec_wan_inst = DEVICE_ETH_WANLAN_INST;
#else
				; /* do nothing */
#endif
#endif
			else if (usb_sec)
#ifdef TR098
				sec_wan_inst = IGD_WANDEVICE_USB_INST;
#endif
#ifdef TR181
				sec_wan_inst = DEVICE_USB_INST;
#endif

#ifdef DSL_ASUSWRT
			else if (dsl_sec)
#ifdef TR098
				sec_wan_inst = IGD_WANDEVICE_DSL_INST;
#endif
#ifdef TR181
				sec_wan_inst = DEVICE_DSL_INST;
#endif				
#endif
			else
				tr_log(LOG_DEBUG, "%s - secondary wan is none", __FUNCTION__);
		}
		else if (lan_pri) {
#ifdef TR098
			pri_wan_inst = IGD_WANDEVICE_LAN_INST;		
#endif
#ifdef TR181
#if defined(ASUSWRT) && defined(RTCONFIG_DUALWAN)
			pri_wan_inst = DEVICE_ETH_WANLAN_INST;
#endif
#endif				
			if (wan_sec)
#ifdef TR098
				sec_wan_inst = IGD_WANDEVICE_WAN_INST;
#endif
#ifdef TR181
#ifdef ASUSWRT
#ifdef RTCONFIG_XDSL
				sec_wan_inst = DEVICE_DSL_INST;
#else					
				sec_wan_inst = DEVICE_ETH_WAN_INST;
#endif
#else 	/* DSL_ASUSWRT */
				sec_wan_inst = DEVICE_ETH_WAN_INST;
#endif
#endif	/* TR181 */	
			else if (usb_sec)
#ifdef TR098
				sec_wan_inst = IGD_WANDEVICE_USB_INST;
#endif
#ifdef TR181	
				sec_wan_inst = DEVICE_USB_INST;
#endif	

#ifdef DSL_ASUSWRT
			else if (dsl_sec)
#ifdef TR098
				sec_wan_inst = IGD_WANDEVICE_DSL_INST;
#endif
#ifdef TR181
				sec_wan_inst = DEVICE_DSL_INST;
#endif				
#endif			
			else
				tr_log(LOG_DEBUG, "%s - secondary wan is none", __FUNCTION__);
		}
		else if (usb_pri) {
#ifdef TR098
			pri_wan_inst = IGD_WANDEVICE_USB_INST;
#endif
#ifdef TR181
			pri_wan_inst = DEVICE_USB_INST;
#endif
			if (wan_sec)
#ifdef TR098
				sec_wan_inst = IGD_WANDEVICE_WAN_INST;
#endif
#ifdef TR181
#ifdef ASUSWRT
#ifdef RTCONFIG_XDSL
				sec_wan_inst = DEVICE_DSL_INST;
#else			
				sec_wan_inst = DEVICE_ETH_WAN_INST;
#endif
#else 	/* DSL_ASUSWRT */
				sec_wan_inst = DEVICE_ETH_WAN_INST;
#endif
#endif	/* TR181 */	
			else if (lan_sec)
#ifdef TR098
				sec_wan_inst = IGD_WANDEVICE_LAN_INST;
#endif
#ifdef TR181
#if defined(ASUSWRT) && defined(RTCONFIG_DUALWAN)
				sec_wan_inst = DEVICE_ETH_WANLAN_INST;
#else
				; /* do nothing */
#endif
#endif

#ifdef DSL_ASUSWRT
			else if (dsl_sec)
#ifdef TR098
				sec_wan_inst = IGD_WANDEVICE_DSL_INST;
#endif
#ifdef TR181
				sec_wan_inst = DEVICE_DSL_INST;
#endif				
#endif
			else
				tr_log(LOG_DEBUG, "%s - secondary wan is none", __FUNCTION__);
		}
		else
			tr_log(LOG_DEBUG, "%s - primary wan is wrong", __FUNCTION__);

		tr_log(LOG_DEBUG, "%s - primary wan is %d", __FUNCTION__, pri_wan_inst);
		tr_log(LOG_DEBUG, "%s - secondary wan is %d", __FUNCTION__, sec_wan_inst);
	}
	else {
#ifdef RTCONFIG_XDSL
		tr_log(LOG_DEBUG, "%s - no dual wan, primary wan is dsl", __FUNCTION__);
#else
		tr_log(LOG_DEBUG, "%s - no dual wan, primary wan is wan", __FUNCTION__);
#endif

#ifdef ASUSWRT
		wan_pri = 1;
#else
		dsl_pri = 1;
#endif

#ifdef TR098
#ifdef ASUSWRT		
		pri_wan_inst = IGD_WANDEVICE_WAN_INST;
#else
		pri_wan_inst = IGD_WANDEVICE_DSL_INST;
#endif
#endif

#ifdef TR181
#ifdef RTCONFIG_XDSL
		pri_wan_inst = DEVICE_DSL_INST;
#else
		pri_wan_inst = DEVICE_ETH_WAN_INST;
#endif
#endif
	}
}

static int get_serial(node_t node, char *arg, char **value)
{
	unsigned char hwaddr[6];
	char buf[13] = {0};
#ifdef ASUSWRT
	ether_atoe(get_lan_hwaddr(), hwaddr);

#else 	/* DSL_ASUSWRT */
	char mac[16] = {0};

	if (tcapi_get("Info_Ether", "mac", mac) < 0)
		return -1;

	ether_atoe(mac, hwaddr);
#endif
	snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X%02X",
		 hwaddr[0], hwaddr[1], hwaddr[2],
		 hwaddr[3], hwaddr[4], hwaddr[5]);

	*value = strdup(buf);
	return *value ? 0 : -1;
} 

static int get_oui(node_t node, char *arg, char **value)
{
	unsigned char hwaddr[6];
	char buf[7] = {0};

#ifdef ASUSWRT
	ether_atoe(get_lan_hwaddr(), hwaddr);

#else 	/* DSL_ASUSWRT */
	char mac[16] = {0};

	if (tcapi_get("Info_Ether", "mac", mac) < 0)
		return -1;

	ether_atoe(mac, hwaddr);
#endif
	snprintf(buf, sizeof(buf), "%02X%02X%02X",
		 hwaddr[0], hwaddr[1], hwaddr[2]);

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_firmver(node_t node, char *arg, char **value)
{
	char buf[32] = {0};
#ifdef DSL_ASUSWRT
	char tmp[16] = {0};
#endif

	snprintf(buf, sizeof(buf), 
#ifdef ASUSWRT
		"%s.%s_%s", nvram_safe_get("firmver"), nvram_safe_get("buildno"), nvram_safe_get("extendno")
#else 	/* DSL_ASUSWRT */
		"%s", tcapi_get_string("DeviceInfo", "FwVer", tmp)
#endif
		);

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_uptime(node_t node, char *arg, char **value)
{
	struct sysinfo info;
	char uptime[32] = {0};

	sysinfo(&info);
	snprintf(uptime, sizeof(uptime), "%lu", info.uptime);

	*value = strdup(uptime);
	return *value ? 0 : -1;
}

static int get_currentlocaltime(node_t node, char *arg, char **value)
{
	char buf[32] = {0};

	snprintf(buf, sizeof(buf), "%s", lib_current_time());

	*value = strdup(buf);
	return *value ? 0 : -1;
}

#ifdef ASUSWRT
static int set_localtimezonename(node_t node, char *arg, char *value)
{
	char buf[64] = {0}, *dst = buf;

	snprintf(buf, sizeof(buf), "%s", value);
	strsep(&dst, ",");

	if (dst && *dst) {
		nvram_set_int("time_zone_dst", 1);
		nvram_set("time_zone_dstoff", dst);
	} else
		nvram_set_int("time_zone_dst", 0);
	
	return set_nvram("time_zone", buf);
}

static int get_localtimezonename(node_t node, char *arg, char **value)
{
	char buf[64] = {0};

	if (nvram_get_int("time_zone_dst"))
		snprintf(buf, sizeof(buf), "%s,%s", nvram_safe_get("time_zone"), nvram_safe_get("time_zone_dstoff"));
	else
		snprintf(buf, sizeof(buf), "%s", nvram_safe_get("time_zone"));

	*value = strdup(buf);
	return *value ? 0 : -1;
}
#endif	/* ASUSWRT */

#ifdef DSL_ASUSWRT
static int set_localtimezone(node_t node, char *arg, char *value)
{
	char buf[16] = {0};

	if (strlen(value)) {
		if (!strcmp(value, "00:00"))
			snprintf(buf, sizeof(buf), "GMT");
		else
			snprintf(buf, sizeof(buf), "GMT%s", value);
	}
	else
		return -1;

	return set_nvram(TIMEZONE_NODE, "TZ", buf);
}

static int get_localtimezone(node_t node, char *arg, char **value)
{
	char buf[16] = {0}, timezone[16] = {0};
	char *ptr = NULL;

	tcapi_get(TIMEZONE_NODE, "TZ", timezone);

	if (strlen(timezone)) {
		if (!strcmp(timezone, "GMT"))
			snprintf(buf, sizeof(buf), "00:00");
		else 
		{
			ptr = timezone;
			ptr += 3;	/* passs GMT string */
			snprintf(buf, sizeof(buf), "%s", ptr);
		}
	}

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_daylightsavingsused(node_t node, char *arg, char *value)
{
	char *value_conv = NULL;

	value_conv = (!war_strcasecmp(value, "1") || !war_strcasecmp(value, "true")) ? "1" : "0";

	return set_nvram(TIMEZONE_NODE, "DAYLIGHT", !strcmp(value_conv, "1") ? "Enable" : "Disable");
}

static int get_daylightsavingsused(node_t node, char *arg, char **value)
{
	char buf[16] = {0};

	snprintf(buf, sizeof(buf), "%s", tcapi_match(TIMEZONE_NODE, "DAYLIGHT", "Enable") ? "true" : "false");

	*value = strdup(buf);
	return *value ? 0 : -1;
}
#endif	/* DSL_ASUSWRT */

static int get_conn_url(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0}, tmp[32] = {0}, buf[64] = {0};
	int unit = 0;

#ifdef ASUSWRT
	if(sw_mode() == SW_MODE_ROUTER) {
#ifdef RTCONFIG_DUALWAN
		unit = wan_primary_ifunit();
#endif
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		snprintf(buf, sizeof(buf), "http://%s:%d",
			nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)),
			nvram_get_int("tr_conn_port") ? : 7547);
	}
	else
	{
		snprintf(buf, sizeof(buf), "http://%s:%d",
			nvram_safe_get("lan_ipaddr"),
			nvram_get_int("tr_conn_port") ? : 7547);
	}
#else 	/* DSL_ASUSWRT */
	char get_buf[64] = {0};
	unit = tcapi_get_int(WANDUCK_NODE, "wan_primary");

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	snprintf(buf, sizeof(buf), "http://%s:%d",
			tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "ipaddr", tmp), get_buf),
			tcapi_get_int(TR069_NODE, "tr_conn_port") ? : 7547);
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

/* for wifi stats */
#ifdef DSL_ASUSWRT
char *get_wifi_ifname(char *prefix, char *ifname)
{
	char *p = NULL;

	p = strchr(prefix, '.');
	if (p != NULL) {
		p++;
		prefix[strlen(prefix) - 1] = '\0';

		if (!strncmp(prefix, "wl0", 3))
#if defined(TCSUPPORT_WLAN_RT6856)
			sprintf(ifname, "ra00_%s", p);
#else
			sprintf(ifname, "ra%s", p);
#endif
		else if (!strncmp(prefix, "wl1", 3))
#if defined(TCSUPPORT_WLAN_RT6856)
			sprintf(ifname, "ra01_%s", p);
#else
			sprintf(ifname, "rai%s", p);
#endif
	}
	else
	{
		if (!strncmp(prefix, "wl0", 3))
#if defined(TCSUPPORT_WLAN_RT6856)
			sprintf(ifname, "ra00_0");
#else
			sprintf(ifname, "ra0");
#endif
		else if (!strncmp(prefix, "wl1", 3))
#if defined(TCSUPPORT_WLAN_RT6856)
			sprintf(ifname, "ra01_0");
#else
			sprintf(ifname, "rai0");
#endif 
	}

	return ifname;
}
#endif

static int get_wlan_totalbytessent(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

#ifdef TR098
	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));
#else
	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
#endif

#ifdef ASUSWRT
#ifdef RTCONFIG_QTN
	if (!strncmp(prefix, "wl1", 3))
		snprintf(buf, sizeof(buf), "%lu", (unsigned long) get_statistic_of_qtn_dev(prefix, ETHERNET_BYTES_SENT));
	else
#endif
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_BYTES_SENT));
#else
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_BYTES_SENT));
#endif	
	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_wlan_totalbytesreceived(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

#ifdef TR098
	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));
#else
	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
#endif

#ifdef ASUSWRT
#ifdef RTCONFIG_QTN
	if (!strncmp(prefix, "wl1", 3))
		snprintf(buf, sizeof(buf), "%lu", (unsigned long) get_statistic_of_qtn_dev(prefix, ETHERNET_BYTES_RECEIVED));
	else
#endif
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_BYTES_RECEIVED));
#else
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_BYTES_RECEIVED));
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_wlan_totalpacketssent(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

#ifdef TR098
	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));
#else
	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
#endif

#ifdef ASUSWRT
#ifdef RTCONFIG_QTN
	if (!strncmp(prefix, "wl1", 3))
		snprintf(buf, sizeof(buf), "%lu", (unsigned long) get_statistic_of_qtn_dev(prefix, ETHERNET_PACKETS_SENT));
	else
#endif
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_PACKETS_SENT));
#else
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_PACKETS_SENT));
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_wlan_totalpacketsreceived(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

#ifdef TR098
	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));
#else
	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
#endif

#ifdef ASUSWRT
#ifdef RTCONFIG_QTN
	if (!strncmp(prefix, "wl1", 3))
		snprintf(buf, sizeof(buf), "%lu", (unsigned long) get_statistic_of_qtn_dev(prefix, ETHERNET_PACKETS_RECEIVED));
	else
#endif
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_PACKETS_RECEIVED));
#else
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_PACKETS_RECEIVED));
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_wlan_errorssent(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

#ifdef TR098
	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));
#else
	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
#endif

#ifdef ASUSWRT
#ifdef RTCONFIG_QTN
	if (!strncmp(prefix, "wl1", 3))
		snprintf(buf, sizeof(buf), "%lu", (unsigned long) get_statistic_of_qtn_dev(prefix, ETHERNET_ERRORS_SENT));
	else
#endif
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_ERRORS_SENT));
#else
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_ERRORS_SENT));
#endif
	
	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_wlan_errorsreceived(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

#ifdef TR098
	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));
#else
	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
#endif

#ifdef ASUSWRT
#ifdef RTCONFIG_QTN
	if (!strncmp(prefix, "wl1", 3))
		snprintf(buf, sizeof(buf), "%lu", (unsigned long) get_statistic_of_qtn_dev(prefix, ETHERNET_ERRORS_RECEIVED));
	else
#endif
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_ERRORS_RECEIVED));
#else
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_ERRORS_RECEIVED));
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_wlan_discardpacketssent(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

#ifdef TR098
	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));
#else
	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
#endif

#ifdef ASUSWRT
#ifdef RTCONFIG_QTN
	if (!strncmp(prefix, "wl1", 3))
		snprintf(buf, sizeof(buf), "%lu", (unsigned long) get_statistic_of_qtn_dev(prefix, ETHERNET_DISCARD_PACKETS_SENT));
	else
#endif
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_DISCARD_PACKETS_SENT));
#else
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_DISCARD_PACKETS_SENT));
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_wlan_discardpacketsreceived(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

#ifdef TR098
	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));
#else
	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
#endif

#ifdef ASUSWRT
#ifdef RTCONFIG_QTN
	if (!strncmp(prefix, "wl1", 3))
		snprintf(buf, sizeof(buf), "%lu", (unsigned long) get_statistic_of_qtn_dev(prefix, ETHERNET_DISCARD_PACKETS_RECEIVED));
	else
#endif
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_DISCARD_PACKETS_RECEIVED));
#else
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_DISCARD_PACKETS_RECEIVED));
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}
/* for wan ip */

char *get_diag_ifname_para(char *path, char *ifname)
{
#ifdef TR098
	if(strstr(path, "WANIPConnection") != NULL)
#endif
#ifdef TR181
	char buf[64] = {0}, lowerlayer[64] = {0};

	snprintf(buf, sizeof(buf), "%s.Lowerlayers", path);
	__get_parameter_value(buf, lowerlayer);
	if (strstr(lowerlayer, "PPP"))
		sprintf(ifname, "pppoe_ifname");
	else if (strstr(path, "IP") != NULL || strstr(path, "Ethernet") != NULL)
#endif
		sprintf(ifname, "ifname");
#ifdef TR098
	else if (strstr(path, "WANPPPConnection") != NULL)
#endif
#ifdef TR181
	else if (strstr(path, "PPP") != NULL)
#endif
		sprintf(ifname, "pppoe_ifname");
	else
		sprintf(ifname, "ifname");

	return ifname;
}

char *get_eth_ifname_para(char *path, char *ifname)
{
#ifdef TR098
	if (strstr(path, "WANIPConnection") != NULL)
#endif
#ifdef TR181
	if (strstr(path, "IP") != NULL || strstr(path, "Ethernet") != NULL)
#endif
		sprintf(ifname, "ifname");
#ifdef TR098
	else if (strstr(path, "WANPPPConnection") != NULL)
#endif
#ifdef TR181
	else if (strstr(path, "PPP") != NULL)
#endif
		sprintf(ifname, "pppoe_ifname");
	else
		sprintf(ifname, "ifname");

	return ifname;
}

static int get_eth_bytessent(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[16] = {0}, tmp[32] = {0}, ifname[32] = {0};
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char dev_ifname[16] = {0};
#endif

	memset(tmp, 0x0, sizeof(tmp));
	memset(prefix, 0, sizeof(prefix));
#ifdef TR098
	sprintf(prefix, "%s", eth_wanip_prefix_by_path(path, tmp));
#endif
#ifdef TR181
	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));
#endif

	sprintf(ifname, "%s", get_eth_ifname_para(path, tmp));
	if(strlen(prefix) == 0 || strlen(ifname) == 0)
		return -1;

#ifdef TR181
	if (!strncmp(prefix, "lan", 3) && strstr(path, ETH_IF))
		snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(DEF_ETH_LAN_IFNAME, ETHERNET_BYTES_SENT));
	else
#endif

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, ifname, tmp)), ETHERNET_BYTES_SENT));
#else 	/* DSL_ASUSWRT */
	{
	tcapi_get(WANDUCK_NODE, strcat_r(prefix, ifname, tmp), dev_ifname);
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(dev_ifname, ETHERNET_BYTES_SENT));
	}
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_eth_bytesreceived(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[16] = {0}, tmp[32] = {0}, ifname[32] = {0};
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char dev_ifname[16] = {0};
#endif

	memset(tmp, 0x0, sizeof(tmp));
	memset(prefix, 0, sizeof(prefix));
#ifdef TR098
	sprintf(prefix, "%s", eth_wanip_prefix_by_path(path, tmp));
#endif
#ifdef TR181
	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));
#endif

	sprintf(ifname, "%s", get_eth_ifname_para(path, tmp));
	if(strlen(prefix) == 0 || strlen(ifname) == 0)
		return -1;

#ifdef TR181
	if (!strncmp(prefix, "lan", 3) && strstr(path, ETH_IF))
		snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(DEF_ETH_LAN_IFNAME, ETHERNET_BYTES_RECEIVED));
	else
#endif

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, ifname, tmp)), ETHERNET_BYTES_RECEIVED));
#else 	/* DSL_ASUSWRT */
	{
	tcapi_get(WANDUCK_NODE, strcat_r(prefix, ifname, tmp), dev_ifname);
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(dev_ifname, ETHERNET_BYTES_RECEIVED));
	}
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_eth_packetssent(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[16] = {0}, tmp[32] = {0}, ifname[32] = {0};
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char dev_ifname[16] = {0};
#endif

	memset(tmp, 0x0, sizeof(tmp));
	memset(prefix, 0, sizeof(prefix));	
#ifdef TR098
	sprintf(prefix, "%s", eth_wanip_prefix_by_path(path, tmp));
#endif
#ifdef TR181
	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));
#endif

	sprintf(ifname, "%s", get_eth_ifname_para(path, tmp));
	if(strlen(prefix) == 0 || strlen(ifname) == 0)
		return -1;

#ifdef TR181
	if (!strncmp(prefix, "lan", 3) && strstr(path, ETH_IF))
		snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(DEF_ETH_LAN_IFNAME, ETHERNET_PACKETS_SENT));
	else
#endif

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, ifname, tmp)), ETHERNET_PACKETS_SENT));
#else 	/* DSL_ASUSWRT */
	{
	tcapi_get(WANDUCK_NODE, strcat_r(prefix, ifname, tmp), dev_ifname);
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(dev_ifname, ETHERNET_PACKETS_SENT));
	}
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_eth_packetsreceived(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[16] = {0}, tmp[32] = {0}, ifname[32] = {0};
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char dev_ifname[16] = {0};
#endif

	memset(tmp, 0x0, sizeof(tmp));
	memset(prefix, 0, sizeof(prefix));	
#ifdef TR098
	sprintf(prefix, "%s", eth_wanip_prefix_by_path(path, tmp));
#endif
#ifdef TR181
	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));
#endif

	sprintf(ifname, "%s", get_eth_ifname_para(path, tmp));
	if(strlen(prefix) == 0 || strlen(ifname) == 0)
		return -1;

#ifdef TR181
	if (!strncmp(prefix, "lan", 3) && strstr(path, ETH_IF))
		snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(DEF_ETH_LAN_IFNAME, ETHERNET_PACKETS_RECEIVED));
	else
#endif
	
#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, ifname, tmp)), ETHERNET_PACKETS_RECEIVED));
#else 	/* DSL_ASUSWRT */
	{
	tcapi_get(WANDUCK_NODE, strcat_r(prefix, ifname, tmp), dev_ifname);
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(dev_ifname, ETHERNET_PACKETS_RECEIVED));
	}
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_eth_errorssent(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[16] = {0}, tmp[32] = {0}, ifname[32] = {0};
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char dev_ifname[16] = {0};
#endif

	memset(tmp, 0x0, sizeof(tmp));
	memset(prefix, 0, sizeof(prefix));	
#ifdef TR098
	sprintf(prefix, "%s", eth_wanip_prefix_by_path(path, tmp));
#endif
#ifdef TR181
	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));
#endif

	sprintf(ifname, "%s", get_eth_ifname_para(path, tmp));
	if(strlen(prefix) == 0 || strlen(ifname) == 0)
		return -1;

#ifdef TR181
	if (!strncmp(prefix, "lan", 3) && strstr(path, ETH_IF))
		snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(DEF_ETH_LAN_IFNAME, ETHERNET_ERRORS_SENT));
	else
#endif

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, ifname, tmp)), ETHERNET_ERRORS_SENT));
#else 	/* DSL_ASUSWRT */
	{
	tcapi_get(WANDUCK_NODE, strcat_r(prefix, ifname, tmp), dev_ifname);
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(dev_ifname, ETHERNET_ERRORS_SENT));
	}
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_eth_errorsreceived(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[16] = {0}, tmp[32] = {0}, ifname[32] = {0};
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char dev_ifname[16] = {0};
#endif

	memset(tmp, 0x0, sizeof(tmp));
	memset(prefix, 0, sizeof(prefix));	
#ifdef TR098
	sprintf(prefix, "%s", eth_wanip_prefix_by_path(path, tmp));
#endif
#ifdef TR181
	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));
#endif

	sprintf(ifname, "%s", get_eth_ifname_para(path, tmp));
	if(strlen(prefix) == 0 || strlen(ifname) == 0)
		return -1;

#ifdef TR181
	if (!strncmp(prefix, "lan", 3) && strstr(path, ETH_IF))
		snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(DEF_ETH_LAN_IFNAME, ETHERNET_ERRORS_RECEIVED));
	else
#endif

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, ifname, tmp)), ETHERNET_ERRORS_RECEIVED));
#else 	/* DSL_ASUSWRT */
	{
	tcapi_get(WANDUCK_NODE, strcat_r(prefix, ifname, tmp), dev_ifname);
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(dev_ifname, ETHERNET_ERRORS_RECEIVED));
	}
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_eth_discardpacketssent(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[16] = {0}, tmp[32] = {0}, ifname[32] = {0};
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char dev_ifname[16] = {0};
#endif

	memset(tmp, 0x0, sizeof(tmp));
	memset(prefix, 0, sizeof(prefix));	
#ifdef TR098
	sprintf(prefix, "%s", eth_wanip_prefix_by_path(path, tmp));
#endif
#ifdef TR181
	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));
#endif

	sprintf(ifname, "%s", get_eth_ifname_para(path, tmp));
	if(strlen(prefix) == 0 || strlen(ifname) == 0)
		return -1;

#ifdef TR181
	if (!strncmp(prefix, "lan", 3) && strstr(path, ETH_IF))
		snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(DEF_ETH_LAN_IFNAME, ETHERNET_DISCARD_PACKETS_SENT));
	else
#endif
	
#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, ifname, tmp)), ETHERNET_DISCARD_PACKETS_SENT));
#else 	/* DSL_ASUSWRT */
	{
	tcapi_get(WANDUCK_NODE, strcat_r(prefix, ifname, tmp), dev_ifname);
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(dev_ifname, ETHERNET_DISCARD_PACKETS_SENT));
	}
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_eth_discardpacketsreceived(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[16] = {0}, tmp[32] = {0}, ifname[32] = {0};
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char dev_ifname[16] = {0};
#endif

	memset(tmp, 0x0, sizeof(tmp));
	memset(prefix, 0, sizeof(prefix));	
#ifdef TR098
	sprintf(prefix, "%s", eth_wanip_prefix_by_path(path, tmp));
#endif
#ifdef TR181	
	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));
#endif

	sprintf(ifname, "%s", get_eth_ifname_para(path, tmp));
	if(strlen(prefix) == 0 || strlen(ifname) == 0)
		return -1;

#ifdef TR181
	if (!strncmp(prefix, "lan", 3) && strstr(path, ETH_IF))
		snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(DEF_ETH_LAN_IFNAME, ETHERNET_DISCARD_PACKETS_RECEIVED));
	else
#endif

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, ifname, tmp)), ETHERNET_DISCARD_PACKETS_RECEIVED));
#else 	/* DSL_ASUSWRT */
	{
	tcapi_get(WANDUCK_NODE, strcat_r(prefix, ifname, tmp), dev_ifname);
	snprintf(buf, sizeof(buf), "%lu", get_statistic_of_net_dev(dev_ifname, ETHERNET_DISCARD_PACKETS_RECEIVED));
	}
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

/* general function for TR098 and TR181 */
static int set_lanhost_dnsservers(node_t node, char *arg, char *value)
{
	int res = 0;
	char *ptr = NULL;

	if((ptr = strchr(value, ',')) != NULL) {
		char *dns;
		dns = strsep(&value, ",");
#ifdef ASUSWRT
		res = set_nvram("dhcp_dns1_x", dns);
#else 	/* DSL_ASUSWRT */
		res = set_nvram(DPROXY_NODE, "Primary_DNS", dns);
#endif
	}
	else
#ifdef ASUSWRT
		res = set_nvram("dhcp_dns1_x", value);
#else 	/* DSL_ASUSWRT */
		res = set_nvram(DPROXY_NODE, "Primary_DNS", value);
#endif

	return res;
}

static int get_lanhost_dnsservers(node_t node, char *arg, char **value)
{
	char buf[32] = {0};

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get("dhcp_dns1_x"));
#else 	/* DSL_ASUSWRT */
	char tmp[32] = {0};

	snprintf(buf, sizeof(buf), "%s", tcapi_get_string(DPROXY_NODE, "Primary_DNS", tmp));
#endif
	
	*value = strdup(buf);
	return *value ? 0 : -1;
}

char *convert_vif_to_sif(char *iface, char *buf)
{
#ifdef ASUSWRT
	char *wan_proto = NULL;
#else 	/* DSL_ASUSWRT */
	char wan_proto[8] = {0};
#endif
	char prefix[16] = {0}, tmp[32] = {0};

#ifdef TR098
	if (strstr(iface, "WANDevice")) {
		sprintf(prefix, "%s", eth_wanip_prefix_by_path(iface, tmp));

		if (strlen(prefix) == 0)
			return buf;

#ifdef ASUSWRT
		wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
#else 	/* DSL_ASUSWRT */
		tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);
#endif

#ifdef ASUSWRT
		if(!strcmp(wan_proto, "dhcp") || !strcmp(wan_proto, "static")
#ifdef RTCCONFIG_XDSL
			|| !strcmp(wan_proto, "ipoa") || !strcmp(wan_proto, "mer")
#endif
		)
#else 	/* DSL_ASUSWRT */
		if(!strcmp(wan_proto, "dhcp") || !strcmp(wan_proto, "static") || !strcmp(wan_proto, "pppoe"))
#endif 
		{	/* DHCP, Static */
			sprintf(buf, "WAN");
		}
		else	/* PPPoE, PPTP, L2TP */
		{
			if(strstr(iface, "WANIPConnection"))
				sprintf(buf, "MAN");
			else if(strstr(iface, "WANPPPConnection"))
				sprintf(buf, "WAN");
			else
				sprintf(buf, "WAN");
		}
	}
	else if (strstr(iface, "LANDevice"))
	{
		sprintf(buf, "LAN");
	}
#endif	//#ifdef TR098

#ifdef TR181
	sprintf(prefix, "%s", ethernet_prefix_by_path(iface, tmp));

	if (strlen(prefix)) {	
		if (!strncmp(prefix, "lan", 3)) {
			sprintf(buf, "LAN");
		}
		else if (!strncmp(prefix, "wan", 3))
		{
			char buf_tmp[64] = {0};
			char lowerlayer[64] = {0};

			snprintf(buf_tmp, sizeof(buf_tmp), "%s.LowerLayers", iface);
			__get_parameter_value(buf_tmp, lowerlayer);

			if (strlen(lowerlayer)) {
#ifdef ASUSWRT
				wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
#else 	/* DSL_ASUSWRT */
				tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);
#endif

				if (strstr(lowerlayer, ETH_IF)) {
#ifdef ASUSWRT
					if (!strcmp(wan_proto, "dhcp") || !strcmp(wan_proto, "static"))
#else 	/* DSL_ASUSWRT */
					if (!strcmp(wan_proto, "dhcp") || !strcmp(wan_proto, "static") || !strcmp(wan_proto, "pppoe"))
#endif
						sprintf(buf, "WAN");
					else if (!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "pptp") || !strcmp(wan_proto, "l2tp"))
						sprintf(buf, "MAN");
				}
				else if (strstr(lowerlayer, ETH_IF)) {
					if (!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "pptp") || !strcmp(wan_proto, "l2tp"))
						sprintf(buf, "WAN");
				}		 
			}
		}
	}	
#endif

	return buf;
}

int write_forwarding_to_nvram(int skip_index)
{
	node_t *children = NULL;
	node_t node;
	int count = 0;
	char buf[128] = {0};
	char tmp_s[64] = {0};
	char sr_rulelist[1024] = {0};

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	if(lib_resolve_node(L3_FORWARDING, &node) == 0) {
		memset(sr_rulelist, 0, sizeof(sr_rulelist));

		count = lib_get_children(node, &children);
		while(count > 0) {
			char name[16] = {0};
			count--;

			if(lib_get_property(children[count], "name", name) == 0) {
				if(string_is_digits(name) == 1 && (skip_index != atoi(name))) {
					char is_static_route[8] = {0}, ip[16] = {0}, mask[16] = {0}, gateway[16] = {0}, iface[128] = {0}, metric[8] = {0}, siface[8] = {0};
					char sr_rule[64] = {0};
					char tmp[32] = {0};

					memset(sr_rule, 0, sizeof(sr_rule));
					/* get the date of StaticRoute */
					sprintf(buf, "%s.%s.StaticRoute", L3_FORWARDING, name);
					sprintf(is_static_route, "%s", __get_parameter_value(buf, tmp_s));
					if(!strcasecmp(is_static_route, "true") || !strcasecmp(is_static_route, "1")) {
						/* get the data of DestIPAddress */
						sprintf(buf, "%s.%s.DestIPAddress", L3_FORWARDING, name);
						sprintf(ip, "%s", __get_parameter_value(buf, tmp_s));
						
						/* get the data of DestSubnetMask */
						sprintf(buf, "%s.%s.DestSubnetMask", L3_FORWARDING, name);
						sprintf(mask, "%s", __get_parameter_value(buf, tmp_s));

						/* get the data of GatewayIPAddress */
						sprintf(buf, "%s.%s.GatewayIPAddress", L3_FORWARDING, name);
						sprintf(gateway, "%s", __get_parameter_value(buf, tmp_s));

						/* get the data of Interface */
						sprintf(buf, "%s.%s.Interface", L3_FORWARDING, name);
						sprintf(iface, "%s", __get_parameter_value(buf, tmp_s));
						sprintf(siface, "%s", convert_vif_to_sif(iface, tmp));
						
						/* get the data of ForwardingMetric */
						sprintf(buf, "%s.%s.ForwardingMetric", L3_FORWARDING, name);
						sprintf(metric, "%s", __get_parameter_value(buf, tmp_s));

						sprintf(sr_rule, "<%s>%s>%s>%s>%s", ip, mask, gateway, metric, siface);
					}

					if(sr_rule[0] != '\0')
						strcat(sr_rulelist, sr_rule);
				}
			}
		}

		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}

		//set_nvram("sr_rulelist", sr_rulelist);
	}

#ifdef ASUSWRT
	set_nvram("sr_rulelist", sr_rulelist);
#else 	/* DSL_ASUSWRT */
	set_nvram(ROUTE_NODE, "sr_rulelist", sr_rulelist);
#endif

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

int add_dhcpstatic_inform(char *path, char *value)
{
	node_t node;

	__set_parameter_value(path, value);
	lib_resolve_node(path, &node);
	if(node->noc == 1 || node->noc == 2)
		add_inform_parameter(path, 1);

	return 0;
}

int add_dhcp_staticlist_instance(char *mac, char *ip)
{
	char buf[128] = {0};
	int i = 0;

	/* add an instance of dhcp static list */
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "%s.", LANHOST_DHCPSTATICADDR);
	if((i = add_object(buf, strlen(buf))) < 9000) {
		/* set Chaddr, DestIPAddress, DestSubnetMask, GatewayIPAddress, Interface, ForwardingMetric */
		sprintf(buf, "%s.%d.Chaddr", LANHOST_DHCPSTATICADDR, i);
		__set_parameter_value(buf, mac);

		sprintf(buf, "%s.%d.Yiaddr", LANHOST_DHCPSTATICADDR, i);		
		__set_parameter_value(buf, ip);

	}
	else
		return -1;

	return 0;
}

int compare_dhcpstatic_instance(char *mac, char *ip)
{
	node_t *children = NULL;
	node_t node;
	int count = 0;
	int need_add = 1;
	char tmp[64] = {0};

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	if(lib_resolve_node(LANHOST_DHCPSTATICADDR, &node) == 0) {
	    	count = lib_get_children(node, &children);
		
		while(count > 0) {
			char name[16];
			count--;

			if(lib_get_property(children[count], "name", name) == 0) {
				if(string_is_digits(name) == 1) {
					char mac_inst[32], ip_inst[32];
					char mac_buf[128], ip_buf[128];
 
					/* get the data of Chaddr */
					sprintf(mac_buf, "%s.%s.Chaddr", LANHOST_DHCPSTATICADDR, name);
					sprintf(mac_inst, "%s", __get_parameter_value(mac_buf, tmp));

					/* get the data of Yiaddr */
					sprintf(ip_buf, "%s.%s.Yiaddr", LANHOST_DHCPSTATICADDR, name);
					sprintf(ip_inst, "%s", __get_parameter_value(ip_buf, tmp));

					if(!strcmp(mac, mac_inst) || !strcmp(ip, ip_inst)) {
						need_add = 0;
						if(!strcmp(mac, mac_inst) && !strcmp(ip, ip_inst)) {
							/* do nothing */
						}
						else if(strcmp(mac, mac_inst))
							add_dhcpstatic_inform(mac_buf, mac);
						else if(strcmp(ip, ip_inst))
							add_dhcpstatic_inform(ip_buf, ip);
						break;
					}
				}
			}
		}

		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}
	}

	if(need_add)
		add_dhcp_staticlist_instance(mac, ip);

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

int compare_dhcpstatic_nvram(char *mac_inst, char *ip_inst, int inst_index)
{
	char buf[128] = {0};
#ifdef ASUSWRT
	char *nv, *nvp, *b;
	char *mac, *ip;
#else
	int i = 0;
#endif
	int need_del = 1;
	
	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

#ifdef ASUSWRT
	nv = nvp = strdup(nvram_safe_get("dhcp_staticlist"));
	if (nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			if ((vstrsep(b, ">", &mac, &ip) != 2))
				continue;
			
			if(!strcmp(mac, mac_inst) && !strcmp(ip, ip_inst)) {
				need_del = 0;
				break;
			}
		}
		free(nv);
	}
#else
	for (i = 0; i < MAX_STATIC_NUM; i ++) {
		char ip[16] = {0}, mac[20] = {0}, element_name[16] = {0};

		memset(mac, 0, sizeof(mac));
		memset(element_name, 0, sizeof(element_name));
		snprintf(element_name, sizeof(element_name), "%s%d", STATIC_DHCP_NODE, i);

		if (tcapi_get(element_name, "IP", ip) < 0) {
			tr_log(LOG_DEBUG, "%s - fail to get IP of %s", __FUNCTION__, element_name);
			continue;
		}

		if (tcapi_get(element_name, "MAC", mac) < 0) {
			tr_log(LOG_DEBUG, "%s - fail to get MAC of %s", __FUNCTION__, element_name);
			continue;
		}

		if (!strcmp(mac, mac_inst) && !strcmp(ip, ip_inst)) {
				need_del = 0;
				break;
		}
	}
#endif

	if(need_del) {
		//node_t node_tmp;
		sprintf(buf, "%s.%d.", LANHOST_DHCPSTATICADDR, inst_index);
		tr_log(LOG_DEBUG, "The path for delete: %s", buf);
		delete_object(buf, strlen(buf));

		/* Reasign instance name of LANHOST_DHCPSTATICADDR */
		//snprintf(buf, sizeof(buf), "%s.", LANHOST_DHCPSTATICADDR);	
		//if(lib_resolve_node(buf, &node_tmp) == 0)
		//	reasign_instance_name(node_tmp, inst_index, 1);
	}

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

int update_dhcp_static_addr()
{
	char buf[128] = {0}, count_inst[8] = {0}, tmp[64] = {0};
	node_t node;
#ifdef ASUSWRT
	char *nv, *nvp, *b;
	char *mac, *ip;
#else
	int i = 0;
#endif
	int count = 0;
	node_t *children = NULL;

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	/* compare staticlist on tree's node */
#ifdef ASUSWRT
	nv = nvp = strdup(nvram_safe_get("dhcp_staticlist"));
	if (nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			if ((vstrsep(b, ">", &mac, &ip) != 2))
				continue;
			
			compare_dhcpstatic_instance(mac, ip);
		}
		free(nv);
	}
#else
	for (i = 0; i < MAX_STATIC_NUM; i ++) {
		char ip[16] = {0}, mac[20] = {0}, element_name[16] = {0};

		memset(mac, 0, sizeof(mac));
		memset(element_name, 0, sizeof(element_name));
		snprintf(element_name, sizeof(element_name), "%s%d", STATIC_DHCP_NODE, i);

		if (tcapi_get(element_name, "IP", ip) < 0) {
			//tr_log(LOG_DEBUG, "%s - fail to get IP of %s", __FUNCTION__, element_name);
			continue;
		}

		if (tcapi_get(element_name, "MAC", mac) < 0) {
			//tr_log(LOG_DEBUG, "%s - fail to get MAC of %s", __FUNCTION__, element_name);
			continue;
		}

		compare_dhcpstatic_instance(mac, ip);
	}
#endif	

	/* compare staticlist on nvram */
	if(lib_resolve_node(LANHOST_DHCPSTATICADDR, &node) == 0) {
	    	count = lib_get_children(node, &children);
		
		while(count > 0) {
			char name[16] = {0};
			count--;

			if(lib_get_property(children[count], "name", name) == 0) {
		    		if(string_is_digits(name) == 1) {
					char mac_inst[32] = {0}, ip_inst[32] = {0};
 
					/* get the data of Chaddr */
					sprintf(buf, "%s.%s.Chaddr", LANHOST_DHCPSTATICADDR, name);
					sprintf(mac_inst, "%s", __get_parameter_value(buf, tmp));

					/* get the data of Yiaddr */
					sprintf(buf, "%s.%s.Yiaddr", LANHOST_DHCPSTATICADDR, name);
					sprintf(ip_inst, "%s", __get_parameter_value(buf, tmp));

					compare_dhcpstatic_nvram(mac_inst, ip_inst, atoi(name));
		    		}
			}
		}

		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}
	}

	/* set the number of DHCPStaticAddress entries */
	sprintf(buf, "%s", LANHOST_DHCPSTATICADDR_NUM);
	sprintf(count_inst, "%s", __get_parameter_value(buf, tmp));

	lib_resolve_node(LANHOST_DHCPSTATICADDR, &node);
	count = lib_get_children(node, &children);
	if(children) {
		lib_destroy_children(children);
		children = NULL;
	}

	if(count != atoi(count_inst)) {
		sprintf(buf, "%d", count);
		__set_parameter_value(LANHOST_DHCPSTATICADDR_NUM, buf);

		lib_resolve_node(LANHOST_DHCPSTATICADDR_NUM, &node);

		if(node->noc == 1 || node->noc == 2) {
			sprintf(buf, "%s", LANHOST_DHCPSTATICADDR_NUM);
			add_inform_parameter(buf, 1);
		}
	}
	
	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

static int set_l3_config(node_t node, char *arg, char *value)
{
	return write_forwarding_to_nvram(0);
}

static int add_l3_forwarding(node_t node, char *arg, int nin)
{
	node_t node_tmp, *children = NULL;
	char buf[128] = {0};
	char *path = lib_node2path(node->parent);
	int count = 0;
	char count_str[8] = {0};

	path[strlen(path) - 1] = '\0';

	tr_log(LOG_DEBUG, "%s - path (%s), nin (%d)", __FUNCTION__, path, nin);

#ifdef TR098
	snprintf(buf, sizeof(buf), "%s.Forwarding", path);
#endif
#ifdef TR181
	snprintf(buf, sizeof(buf), "%s.IPv4Forwarding", path);		
#endif

	/* Update ForwardNumberOfEntries or IPv4ForwardingNumberOfEntries of path */
	if (lib_resolve_node(buf, &node_tmp) == 0) {
		count = lib_get_children(node_tmp, &children);
		if (children) {
			lib_destroy_children(children);
			children = NULL;
		}

#ifdef TR098
		snprintf(buf, sizeof(buf), "%s.ForwardNumberOfEntries", path);
#endif
#ifdef TR181
		snprintf(buf, sizeof(buf), "%s.IPv4ForwardingNumberOfEntries", path);		
#endif
		snprintf(count_str, sizeof(count_str), "%d", count + 1);
		__set_parameter_value(buf, count_str);
		tr_log(LOG_DEBUG, "%s - Set %s as %s", __FUNCTION__, buf, count_str);
	}

#ifdef ASUSWRT
	set_nvram("sr_enable_x", "1");	/* enable static route */
#else 	/* DSL_ASUSWRT */
	set_nvram(ROUTE_NODE, "sr_enable", "1");
#endif

	return 0;
}

static int del_l3_forwarding(node_t node, char *arg, int nin)
{
	node_t node_tmp, *children = NULL;
	char buf[128] = {0};
	char *path = lib_node2path(node->parent);
	int count = 0;
	char count_str[8] = {0};

	path[strlen(path) - 1] = '\0';

	tr_log(LOG_DEBUG, "%s - path (%s), nin (%d)", __FUNCTION__, path, nin);

#if 0
	sprintf(buf, "%s.%d.StaticRoute", path, nin);
	if (string2boolean(__get_parameter_value(buf, tmp_s)) != BOOLEAN_TRUE) {
		tr_log(LOG_DEBUG, "%s - %s.%d is dynamic route, can't be delete.", __FUNCTION__, path, nin);
		return -1;
	}
#endif

	write_forwarding_to_nvram(nin);
	//nvram_commit();

#ifdef TR098
	snprintf(buf, sizeof(buf), "%s.Forwarding", path);
#endif
#ifdef TR181
	snprintf(buf, sizeof(buf), "%s.IPv4Forwarding", path);		
#endif

	/* Update ForwardNumberOfEntries or IPv4ForwardingNumberOfEntries of path */
	if (lib_resolve_node(buf, &node_tmp) == 0) {
		count = lib_get_children(node_tmp, &children);
		if (children) {
			lib_destroy_children(children);
			children = NULL;
		}

#ifdef TR098
		snprintf(buf, sizeof(buf), "%s.ForwardNumberOfEntries", path);
#endif
#ifdef TR181
		snprintf(buf, sizeof(buf), "%s.IPv4ForwardingNumberOfEntries", path);		
#endif
		snprintf(count_str, sizeof(count_str), "%d", count - 1);
		__set_parameter_value(buf, count_str);
		tr_log(LOG_DEBUG, "%s - Set %s as %s", __FUNCTION__, buf, count_str);
	}

	/* Reasign instance name of L3_FORWARDING */
	//reasign_instance_name(node, nin, 0);

	return 0;
}

char *convert_sif_to_vif(char *iface, char *buf)
{
	char buf_tmp[128] = {0};

	if (!strcmp(iface, "WAN") || !strcmp(iface, "MAN")) {
#ifdef ASUSWRT
		char *wan_proto = NULL;
		int unit = wan_primary_ifunit();
#else 	/* DSL_ASUSWRT */
		char wan_proto[8] = {0};
		int unit = tcapi_get_int(WANDUCK_NODE, "wan_primary");
#endif
		char prefix[sizeof("wanXXXXXXXXXX_")] = {0}, tmp[32] = {0};
#ifdef TR098		
		int inst_num = 0;
		int wd_inst = get_wd_inst_by_wan_unit(unit);
#endif

		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
#ifdef ASUSWRT
		wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
#else 	/* DSL_ASUSWRT */
		tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);
#endif

		if(!strcmp(wan_proto, "dhcp") || !strcmp(wan_proto, "static")
#if defined(ASUWRT) && defined(RTCONFIG_XDSL)
			|| !strcmp(wan_proto, "ipoa") || !strcmp(wan_proto, "mer")
#endif
		) {	/* DHCP, Static */
#ifdef TR098
#ifdef RTCONFIG_XDSL
			int wcd_inst = 0;

#ifdef ASUSWRT
			if (wd_inst == IGD_WANDEVICE_WAN_INST)
#else 	/* DSL_ASUSWRT */				
			if (wd_inst == IGD_WANDEVICE_DSL_INST)
#endif
			{

#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
				if (nvram_match("dslx_transmode", ADSL_TRANS_MODE))
					snprintf(prefix, sizeof(prefix), "dsl%d", ADSL_INTERNET_INDEX);
#endif
#ifdef RTCONFIG_VDSL
				if (nvram_match("dslx_transmode", VDSL_TRANS_MODE))
					snprintf(prefix, sizeof(prefix), "dsl%d", VDSL_INTERNET_INDEX);
#endif
#else 	/* DSL_ASUSWRT */
#ifdef TCSUPPORT_WAN_ATM
				if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", ADSL_TRANS_MODE)) || !support_adsl_vdsl)
					snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, ADSL_INTERNET_INDEX);
#endif
#ifdef TCSUPPORT_WAN_PTM
				if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", VDSL_TRANS_MODE)) || !support_adsl_vdsl)
					snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, VDSL_INTERNET_INDEX);
#endif
#endif

				if ((wcd_inst = find_xdsl_wan_device(wd_inst, prefix))) {
					/* get instance number of WANIPConnection */
					snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d.WANConnectionDevice.%d.WANIPConnection", IGD_WANDEVICE, wd_inst, wcd_inst);
					inst_num = get_inst_num_by_path(buf_tmp);

					if (inst_num)
						sprintf(buf, "%s.%d.WANConnectionDevice.%d.WANIPConnection.%d", IGD_WANDEVICE, wd_inst, wcd_inst, inst_num);					
				}
			}
			else
#endif	/* RTCONFIG_XDSL */
			{
				/* get instance number of WANIPConnection */
				snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d.WANConnectionDevice.1.WANIPConnection", IGD_WANDEVICE, wd_inst);
				inst_num = get_inst_num_by_path(buf_tmp);

				if (inst_num)
					sprintf(buf, "%s.%d.WANConnectionDevice.1.WANIPConnection.%d", IGD_WANDEVICE, wd_inst, inst_num);
			}
#endif	/* TR098 */

#ifdef TR181
			int ip_if_inst = 0;
#ifdef RTCONFIG_XDSL
			int inst_index = 0;
			if (pri_wan_inst == DEVICE_DSL_INST) {
#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
				if (nvram_match("dslx_transmode", ADSL_TRANS_MODE)) {
					snprintf(prefix, sizeof(prefix), "dsl%d", ADSL_INTERNET_INDEX);
					if ((inst_index = find_xtm_link(ADSL_ID, prefix)))
						snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d", ATM_LINK, inst_index);
				}
#endif
#ifdef RTCONFIG_VDSL
				if (nvram_match("dslx_transmode", VDSL_TRANS_MODE)) {
					snprintf(prefix, sizeof(prefix), "dsl%d", VDSL_INTERNET_INDEX);
					if ((inst_index = find_xtm_link(VDSL_ID, prefix)))
						snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d", PTM_LINK, inst_index);
				}
#endif

#else 	/* DSL_ASUSWRT */
#ifdef TCSUPPORT_WAN_ATM
				if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", ADSL_TRANS_MODE)) || !support_adsl_vdsl) {
					snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, ADSL_INTERNET_INDEX);	
					if ((inst_index = find_xtm_link(ADSL_ID, prefix)))
						snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d", ATM_LINK, inst_index);
				}
#endif

#ifdef TCSUPPORT_WAN_PTM
				if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", VDSL_TRANS_MODE)) || !support_adsl_vdsl) { 
					snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, VDSL_INTERNET_INDEX);
					if ((inst_index = find_xtm_link(ADSL_ID, prefix)))
						snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d", ATM_LINK, inst_index);
				}
#endif
#endif
			}
			else
#endif	/* RTCONFIG_XDSL */
			snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d", ETH_IF, pri_wan_inst);

			if ((ip_if_inst = search_ip_ppp_lowerlayer(buf_tmp, IP_FLAG)))
				sprintf(buf, "%s.%d", IP_IF, ip_if_inst);
#endif	/* TR181 */
		}
		else	/* PPPoE, PPTP, L2TP */
		{
			if(!strcmp(iface, "WAN")) {
#ifdef TR098
#ifdef RTCONFIG_XDSL
				int wcd_inst = 0;

#ifdef ASUSWRT				
				if (wd_inst == IGD_WANDEVICE_WAN_INST)
#else 	/* DSL_ASUSWRT */
				if (wd_inst == IGD_WANDEVICE_DSL_INST)					
#endif
				{

#ifdef ASUSWRT	
#ifdef RTCONFIG_DSL
					if (nvram_match("dslx_transmode", ADSL_TRANS_MODE))
						snprintf(prefix, sizeof(prefix), "dsl%d", ADSL_INTERNET_INDEX);
#endif
#ifdef RTCONFIG_VDSL
					if (nvram_match("dslx_transmode", VDSL_TRANS_MODE))
						snprintf(prefix, sizeof(prefix), "dsl%d", VDSL_INTERNET_INDEX);
#endif

#else 	/* DSL_ASUSWRT */

#ifdef TCSUPPORT_WAN_ATM
				if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", ADSL_TRANS_MODE)) || !support_adsl_vdsl)
					snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, ADSL_INTERNET_INDEX);
#endif
#ifdef TCSUPPORT_WAN_PTM
				if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", VDSL_TRANS_MODE)) || !support_adsl_vdsl)
					snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, VDSL_INTERNET_INDEX);
#endif						
#endif	

					if ((wcd_inst = find_xdsl_wan_device(wd_inst, prefix))) {
						/* get instance number of WANIPConnection */
						snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d.WANConnectionDevice.%d.WANPPPConnection", IGD_WANDEVICE, wd_inst, wcd_inst);
						inst_num = get_inst_num_by_path(buf_tmp);

						if (inst_num)
							sprintf(buf, "%s.%d.WANConnectionDevice.%d.WANPPPConnection.%d", IGD_WANDEVICE, wd_inst, wcd_inst, inst_num);					
					}
				}
				else
#endif	/* RTCONFIG_XDSL */			
				{		
					//sprintf(buf, "%s.WANConnectionDevice.%d.WANPPPConnection.1", IGD_WANDEVICE_ETH, unit + 1);
					/* get instance number of WANIPConnection */
					snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d.WANConnectionDevice.1.WANPPPConnection", IGD_WANDEVICE, wd_inst);
					inst_num = get_inst_num_by_path(buf_tmp);

					if (inst_num)
						sprintf(buf, "%s.%d.WANConnectionDevice.1.WANPPPConnection.%d", IGD_WANDEVICE, wd_inst, inst_num);
				}
#endif	/* TR098 */

#ifdef TR181
				//sprintf(buf, "%s.%d", DEVICE_PPP, unit + 1);
				int ip_if_inst = 0, ppp_if_inst = 0;
#ifdef RTCONFIG_XDSL
				int inst_index = 0;
				if (pri_wan_inst == DEVICE_DSL_INST) {
#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
					if (nvram_match("dslx_transmode", ADSL_TRANS_MODE)) {
						snprintf(prefix, sizeof(prefix), "dsl%d", ADSL_INTERNET_INDEX);
						if ((inst_index = find_xtm_link(ADSL_ID, prefix)))
							snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d", ATM_LINK, inst_index);
					}
#endif
#ifdef RTCONFIG_VDSL
					if (nvram_match("dslx_transmode", VDSL_TRANS_MODE)) {
						snprintf(prefix, sizeof(prefix), "dsl%d", VDSL_INTERNET_INDEX);
						if ((inst_index = find_xtm_link(VDSL_ID, prefix)))
							snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d", PTM_LINK, inst_index);
					}
#endif

#else 	/* DSL_ASUSWRT */
#ifdef TCSUPPORT_WAN_ATM
					if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", ADSL_TRANS_MODE)) || !support_adsl_vdsl) {
						snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, ADSL_INTERNET_INDEX);	
						if ((inst_index = find_xtm_link(ADSL_ID, prefix)))
							snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d", ATM_LINK, inst_index);
					}
#endif

#ifdef TCSUPPORT_WAN_PTM
					if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", VDSL_TRANS_MODE)) || !support_adsl_vdsl) { 
						snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, VDSL_INTERNET_INDEX);
						if ((inst_index = find_xtm_link(ADSL_ID, prefix)))
							snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d", ATM_LINK, inst_index);
					}
#endif
#endif
				}
				else
#endif	/* RTCONFIG_XDSL */		
				snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d", ETH_IF, pri_wan_inst);

				if ((ppp_if_inst = search_ip_ppp_lowerlayer(buf_tmp, PPP_FLAG))) {
					snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d", PPP_IF, ppp_if_inst);
					if ((ip_if_inst = search_ip_ppp_lowerlayer(buf_tmp, IP_FLAG)))
						sprintf(buf, "%s.%d", IP_IF, ip_if_inst);
				}				
#endif	/* TR181 */
			}

			if(!strcmp(iface, "MAN")) {
#ifdef TR098
#ifdef RTCONFIG_XDSL
				int wcd_inst = 0;

#ifdef ASUSWRT
				if (wd_inst == IGD_WANDEVICE_WAN_INST) 
#else 	/* DSL_ASUSWRT */
				if (wd_inst == IGD_WANDEVICE_DSL_INST)
#endif
				{

#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
					if (nvram_match("dslx_transmode", ADSL_TRANS_MODE))
						snprintf(prefix, sizeof(prefix), "dsl%d", ADSL_INTERNET_INDEX);
#endif
#ifdef RTCONFIG_VDSL
					if (nvram_match("dslx_transmode", VDSL_TRANS_MODE))
						snprintf(prefix, sizeof(prefix), "dsl%d", VDSL_INTERNET_INDEX);
#endif

#else 	/* DSL_ASUSWRT */

#ifdef TCSUPPORT_WAN_ATM
					if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", ADSL_TRANS_MODE)) || !support_adsl_vdsl)
						snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, ADSL_INTERNET_INDEX);
#endif
#ifdef TCSUPPORT_WAN_PTM
					if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", VDSL_TRANS_MODE)) || !support_adsl_vdsl)
						snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, VDSL_INTERNET_INDEX);
#endif				
#endif	

					if ((wcd_inst = find_xdsl_wan_device(wd_inst, prefix))) {
						/* get instance number of WANIPConnection */
						snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d.WANConnectionDevice.%d.WANIPConnection", IGD_WANDEVICE, wd_inst, wcd_inst);
						inst_num = get_inst_num_by_path(buf_tmp);

						if (inst_num)
							sprintf(buf, "%s.%d.WANConnectionDevice.%d.WANIPConnection.%d", IGD_WANDEVICE, wd_inst, wcd_inst, inst_num);					
					}
				}
				else
#endif	/* RTCONFIG_XDSL */		
				{	
					//sprintf(buf, "%s.WANConnectionDevice.%d.WANIPConnection.1", IGD_WANDEVICE_ETH, unit + 1);
					/* get instance number of WANIPConnection */
					snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d.WANConnectionDevice.1.WANIPConnection", IGD_WANDEVICE, wd_inst);
					inst_num = get_inst_num_by_path(buf_tmp);

					if (inst_num)
						sprintf(buf, "%s.%d.WANConnectionDevice.1.WANIPConnection.%d", IGD_WANDEVICE, wd_inst, inst_num);
				}
#endif	/* TR098 */

#ifdef TR181
				//sprintf(buf, "%s.%d", DEVICE_PPP, unit + 1);
				int ip_if_inst = 0;
#ifdef RTCONFIG_XDSL
				int inst_index = 0;

				if (pri_wan_inst == DEVICE_DSL_INST) {
#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
					if (nvram_match("dslx_transmode", ADSL_TRANS_MODE)) {
						snprintf(prefix, sizeof(prefix), "dsl%d", ADSL_INTERNET_INDEX);
						if ((inst_index = find_xtm_link(ADSL_ID, prefix)))
							snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d", ATM_LINK, inst_index);
					}
#endif
#ifdef RTCONFIG_VDSL
					if (nvram_match("dslx_transmode", VDSL_TRANS_MODE)) {
						snprintf(prefix, sizeof(prefix), "dsl%d", VDSL_INTERNET_INDEX);
						if ((inst_index = find_xtm_link(VDSL_ID, prefix)))
							snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d", PTM_LINK, inst_index);
					}
#endif

#else 	/* DSL_ASUSWRT */
#ifdef TCSUPPORT_WAN_ATM
					if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", ADSL_TRANS_MODE)) || !support_adsl_vdsl) {
						snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, ADSL_INTERNET_INDEX);	
						if ((inst_index = find_xtm_link(ADSL_ID, prefix)))
							snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d", ATM_LINK, inst_index);
					}
#endif

#ifdef TCSUPPORT_WAN_PTM
					if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", VDSL_TRANS_MODE)) || !support_adsl_vdsl) { 
						snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, VDSL_INTERNET_INDEX);
						if ((inst_index = find_xtm_link(ADSL_ID, prefix)))
							snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d", ATM_LINK, inst_index);
					}
#endif
#endif
				}
				else
#endif	/* RTCONFIG_XDSL */	
				snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d", ETH_IF, pri_wan_inst);

				if ((ip_if_inst = search_ip_ppp_lowerlayer(buf_tmp, IP_FLAG)))
					sprintf(buf, "%s.%d", IP_IF, ip_if_inst);				
#endif	/* TR181 */
			}
		}
	}
	else if (!strcmp(iface, "LAN"))
	{
#ifdef TR098
		sprintf(buf, "%s.LANHostConfigManagement.IPInterface.1", LAN_DEVICE);	/* only one lan */
#endif

#ifdef TR181
		int ip_if_inst = 0;

		snprintf(buf_tmp, sizeof(buf_tmp), "%s.%d", DEVICE_BRIDGING_BRIDGE_PORT_ONE, mng_port_index);
		if ((ip_if_inst = search_ip_ppp_lowerlayer(buf_tmp, IP_FLAG)))
			sprintf(buf, "%s.%d", IP_IF, ip_if_inst);
#endif
	}

	return buf;
}

int compare_forwarding_nvram(char *static_route_inst, char *dst_ip_inst, char *dst_netmask_inst, char *gw_ip_inst, char *if_inst, char *metric_inst, int inst_index)
{
	char buf[128] = {0};
	char *nv = NULL, *nvp = NULL, *b = NULL;
	char *ip = NULL, *netmask = NULL, *gateway = NULL, *metric = NULL, *interface = NULL;
	int need_del = 1;
#ifdef DSL_ASUSWRT
	char rulelist_tmp[1024] = {0};
#endif
	
	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

#ifdef ASUSWRT
	nv = nvp = strdup(nvram_safe_get("sr_rulelist"));
#else 	/* DSL_ASUSWRT */
	nv = nvp = strdup(tcapi_get_string(ROUTE_NODE, "sr_rulelist", rulelist_tmp));
#endif
	if (nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			char iface[128] = {0}, tmp[128] = {0};

			if ((vstrsep(b, ">", &ip, &netmask, &gateway, &metric, &interface) != 5))
				continue;

			snprintf(iface, sizeof(iface), "%s", convert_sif_to_vif(interface, tmp));
			//snprintf(iface, sizeof(iface), "%s", convert_vif_to_sif(if_inst, tmp));

			tr_log(LOG_DEBUG, "nvram: %s, %s, %s, %s (%s), %s", ip, netmask, gateway, interface, iface, metric);
			tr_log(LOG_DEBUG, "instance: %s, %s, %s, %s, %s", dst_ip_inst, dst_netmask_inst, gw_ip_inst, if_inst, metric_inst);

			if(!strcmp(ip, dst_ip_inst) && !strcmp(netmask, dst_netmask_inst) && 
					!strcmp(gateway, gw_ip_inst) && !strcmp(metric, metric_inst) &&
					!strcmp(iface, if_inst)) {
				need_del = 0;
				break;
			}
		}
		free(nv);
	}

	if(need_del) {
		//node_t node_tmp;
		//tr_log(LOG_DEBUG, "instance: %s, %s, %s, %s, %s,", dst_ip_inst, dst_netmask_inst, gw_ip_inst, if_inst, metric_inst);
		if(!strcasecmp(static_route_inst, "true") || !strcasecmp(static_route_inst, "1")) {
			sprintf(buf, "%s.%d.", L3_FORWARDING, inst_index);
			tr_log(LOG_DEBUG, "The path for delete: %s", buf);
			delete_object(buf, strlen(buf));

#if 0
			/* Reasign instance name of L3_FORWARDING */
			snprintf(buf, sizeof(buf), "%s.", L3_FORWARDING);	
			if(lib_resolve_node(buf, &node_tmp) == 0)
				reasign_instance_name(node_tmp, inst_index, 1);
#endif
		}
	}

	tr_log(LOG_DEBUG, "%s -end", __FUNCTION__);

	return 0;
}

int add_forwarding_instance(int is_static_route, char *ip, char *netmask, char *gateway, char *interface, int metric)
{
	char buf[128] = {0};
	char metric_str[4] = {0};
	int i = 0;

	tr_log(LOG_DEBUG, "%s - ip (%s), netmask (%s), gateway (%s), iface (%s), metric (%d)", __FUNCTION__, ip, netmask, gateway, interface, metric);
	
	/* add an instance of forwarding */
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "%s.", L3_FORWARDING);
	if((i = add_object(buf, strlen(buf))) < 9000) {
		/* set StaticRoute, DestIPAddress, DestSubnetMask, GatewayIPAddress, Interface, ForwardingMetric */
		sprintf(buf, "%s.%d.StaticRoute", L3_FORWARDING, i);
		is_static_route ? __set_parameter_value(buf, "true") : __set_parameter_value(buf, "false");

		sprintf(buf, "%s.%d.DestIPAddress", L3_FORWARDING, i);
		__set_parameter_value(buf, ip);

		sprintf(buf, "%s.%d.DestSubnetMask", L3_FORWARDING, i);
		__set_parameter_value(buf, netmask);

		sprintf(buf, "%s.%d.GatewayIPAddress", L3_FORWARDING, i);
		__set_parameter_value(buf, gateway);

		sprintf(buf, "%s.%d.Interface", L3_FORWARDING, i);
		__set_parameter_value(buf, interface);

		sprintf(buf, "%s.%d.ForwardingMetric", L3_FORWARDING, i);
		sprintf(metric_str, "%d", metric);
		__set_parameter_value(buf, metric_str);
	}
	else
		return -1;

	return 0;
}

int compare_forwarding_instance(char *ip, char *netmask, char *gateway, char *iface, char *metric)
{
	node_t *children = NULL;
	node_t node;
	int count = 0;
	int need_add = 1;
	//char tmp_s[64] = {0};
	char buf[128] = {0};

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	if(lib_resolve_node(L3_FORWARDING, &node) == 0) {
		count = lib_get_children(node, &children);
		
		while(count > 0) {
			char name[16] = {0};
			count--;

			if(lib_get_property(children[count], "name", name) == 0) {
				if(string_is_digits(name) == 1) {
					char dst_ip_inst[32] = {0}, dst_netmask_inst[32] = {0}, gw_ip_inst[32] = {0}, 
						if_inst[128] = {0}, metric_inst[8] = {0};
					
					/* get the data of DestIPAddress */
					memset(buf, 0, sizeof(buf));
					snprintf(buf, sizeof(buf), "%s.%s.DestIPAddress", L3_FORWARDING, name);
					__get_parameter_value(buf, dst_ip_inst);

					/* get the data of DestSubnetMask */
					memset(buf, 0, sizeof(buf));
					snprintf(buf, sizeof(buf), "%s.%s.DestSubnetMask", L3_FORWARDING, name);
					__get_parameter_value(buf, dst_netmask_inst);

					/* get the data of GatewayIPAddress */
					memset(buf, 0, sizeof(buf));
					snprintf(buf, sizeof(buf), "%s.%s.GatewayIPAddress", L3_FORWARDING, name);
					__get_parameter_value(buf, gw_ip_inst);

					/* get the data of Interface */
					memset(buf, 0, sizeof(buf));
					snprintf(buf, sizeof(buf), "%s.%s.Interface", L3_FORWARDING, name);
					__get_parameter_value(buf, if_inst);
		
					/* get the data of ForwardingMetric */
					memset(buf, 0, sizeof(buf));
					snprintf(buf, sizeof(buf), "%s.%s.ForwardingMetric", L3_FORWARDING, name);
					__get_parameter_value(buf, metric_inst);

					//tr_log(LOG_DEBUG, "nvram: %s, %s, %s, %s, %s,", ip, netmask, gateway, iface, metric);
					//tr_log(LOG_DEBUG, "instance: %s, %s, %s, %s, %s,", dst_ip_inst, dst_netmask_inst, gw_ip_inst, if_inst, metric_inst);

					if(!strcmp(ip, dst_ip_inst) && !strcmp(netmask, dst_netmask_inst) && 
							!strcmp(gateway, gw_ip_inst) && !strcmp(iface, if_inst) && 
							!strcmp(metric, metric_inst)) {
						need_add = 0;
						break;
					}
				}
			}
		}

		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}
	}
	tr_log(LOG_DEBUG, "%s - ip (%s), netmask (%s), gateway (%s), iface (%s), metric (%s)", __FUNCTION__, ip, netmask, gateway, iface, metric);
	if(need_add)
		add_forwarding_instance(1, ip, netmask, gateway, iface, atoi(metric));

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

int update_forwarding()
{
	char buf[128] = {0};
	node_t node;
	char *nv = NULL, *nvp = NULL, *b = NULL;
	char *ip = NULL, *netmask = NULL, *gateway = NULL, *metric = NULL, *interface = NULL;
	int count = 0;
	node_t *children = NULL;
	char count_inst[8] = {0}, tmp[64] = {0};
#ifdef DSL_ASUSWRT
	char rulelist_tmp[1024] = {0};
#endif

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	/* delete the dynamic static first */
#if 0
	if(lib_resolve_node(L3_FORWARDING, &node) == 0) {
		count = lib_get_children(node, &children);

		while(count > 0) {
			char name[32];
			count--;

			if(lib_get_property(children[count], "name", name) == 0) {
				if(string_is_digits( name ) == 1) {
					char static_route_inst[8];

					snprintf(path, sizeof(path), "%s.%s.StaticRoute", L3_FORWARDING, name);
					snprintf(static_route_inst, sizeof(static_route_inst), "%s", __get_parameter_value(path, tmp));
					
					if(!strcasecmp(static_route_inst, "false") || !strcasecmp(static_route_inst, "0")) {
						sprintf(path, "%s.%s.", L3_FORWARDING, name);
						tr_log(LOG_DEBUG, "The dynamic route's path for delete: %s", path);
						delete_object(path, strlen(path));
					}
				}
			}
		}

		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}
	}
	else
		return -1;
#endif

	/* compare forwarding on tree's node */
#ifdef ASUSWRT
	nv = nvp = strdup(nvram_safe_get("sr_rulelist"));
#else 	/* DSL_ASUSWRT */
	nv = nvp = strdup(tcapi_get_string(ROUTE_NODE, "sr_rulelist", rulelist_tmp));
#endif
	if (nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			char iface[128] = {0}, vif[128] = {0};

			if ((vstrsep(b, ">", &ip, &netmask, &gateway, &metric, &interface) != 5))
				continue;

			//if(!strcmp(interface, "LAN"))
			//	continue;
			snprintf(iface, sizeof(iface), "%s", convert_sif_to_vif(interface, vif));

			tr_log(LOG_DEBUG, "%s - ip (%s), netmask (%s), gateway (%s), interface (%s), metric (%s)", __FUNCTION__, ip, netmask, gateway, interface, metric);
			compare_forwarding_instance(ip, netmask, gateway, iface, metric);			
		}
		free(nv);
	}

	/* compare forwarding on nvram */
	if(lib_resolve_node(L3_FORWARDING, &node) == 0) {
		count = lib_get_children(node, &children);
		
		while(count > 0) {
			char name[16];
			count--;

			if(lib_get_property(children[count], "name", name) == 0) {
				if(string_is_digits(name) == 1) {
					char static_route_inst[8] = {0}, dst_ip_inst[32] = {0}, dst_netmask_inst[32] = {0}, 
						gw_ip_inst[32] = {0}, if_inst[128] = {0}, metric_inst[8] = {0};

					memset(dst_ip_inst, 0, sizeof(dst_ip_inst));
					memset(dst_netmask_inst, 0, sizeof(dst_netmask_inst));
					memset(gw_ip_inst, 0, sizeof(gw_ip_inst));
					memset(if_inst, 0, sizeof(if_inst));
					memset(metric_inst, 0, sizeof(metric_inst));
																				
					/* get the data of StaticRoute */
					snprintf(buf, sizeof(buf), "%s.%s.StaticRoute", L3_FORWARDING, name);
					snprintf(static_route_inst, sizeof(static_route_inst), "%s", __get_parameter_value(buf, static_route_inst));
					
					/* get the data of DestIPAddress */
					snprintf(buf, sizeof(buf), "%s.%s.DestIPAddress", L3_FORWARDING, name);
					snprintf(dst_ip_inst, sizeof(dst_ip_inst), "%s", __get_parameter_value(buf, dst_ip_inst));

					/* get the data of DestSubnetMask */
					snprintf(buf, sizeof(buf), "%s.%s.DestSubnetMask", L3_FORWARDING, name);
					snprintf(dst_netmask_inst, sizeof(dst_netmask_inst), "%s", __get_parameter_value(buf, dst_netmask_inst));

					/* get the data of GatewayIPAddress */
					snprintf(buf, sizeof(buf), "%s.%s.GatewayIPAddress", L3_FORWARDING, name);
					snprintf(gw_ip_inst, sizeof(gw_ip_inst), "%s", __get_parameter_value(buf, gw_ip_inst));

					/* get the data of Interface */
					snprintf(buf, sizeof(buf), "%s.%s.Interface", L3_FORWARDING, name);
					snprintf(if_inst, sizeof(if_inst), "%s", __get_parameter_value(buf, if_inst));
		
					/* get the data of ForwardingMetric */
					snprintf(buf, sizeof(buf), "%s.%s.ForwardingMetric", L3_FORWARDING, name);
					snprintf(metric_inst, sizeof(metric_inst), "%s", __get_parameter_value(buf, metric_inst));

					tr_log(LOG_DEBUG, "%s - dst_ip_inst (%s), dst_netmask_inst (%s), gw_ip_inst (%s), if_inst (%s), metric_inst (%s)", __FUNCTION__, dst_ip_inst, dst_netmask_inst, gw_ip_inst, if_inst, metric_inst);
					compare_forwarding_nvram(static_route_inst, dst_ip_inst, dst_netmask_inst, gw_ip_inst, if_inst, metric_inst, atoi(name));
				}
			}
		}

		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}
	}

#if 0 /* crash is here, malloced memory corruption? */
	/* add dynamic route */
	fp = fopen("/proc/net/route", "r");
	if (fp == NULL)
		return -1;

	while ((str = fgets(buf, sizeof(buf), fp)) != NULL) {
		char *dev;
		struct in_addr dest, gw, mask;
		int flags, ref, use, metric_int;
		char iface[16], ip_str[16], mask_str[16], gateway_str[16];

		dev = strsep(&str, " \t");
		if (!str || dev == str)
			continue;
		if (sscanf(str, "%x%x%x%d%u%d%x", &dest.s_addr, &gw.s_addr,
			   &flags, &ref, &use, &metric_int, &mask.s_addr) != 7)
			continue;

		/* Skip interfaces here */
		if (strcmp(dev, "lo") == 0 || strcmp(dev, "br0") == 0)
			continue;

		sprintf(iface, "%s", dev);
		sprintf(ip_str, "%s", inet_ntoa(dest));
		sprintf(mask_str, "%s", inet_ntoa(mask));
		sprintf(gateway_str, "%s", inet_ntoa(gw));

		if(!forwarding_instance_exist(ip_str)) {
			char iface_str[128];
			snprintf(iface_str, sizeof(iface_str), "%s", convert_rif_to_vif(iface));
			if(add_forwarding_instance(0, ip_str, mask_str, gateway_str, iface_str, metric_int))
				tr_log(LOG_DEBUG, "failed to add forwarding instance: %s, %s, %s, %s, %d", ip_str, mask_str, gateway_str, iface_str, metric_int);
		}
	}
	fclose(fp);
#endif

	/* set the number of Forwarding entries */
	snprintf(buf, sizeof(buf), "%s", L3_FORWARDING_NUM);
	snprintf(count_inst, sizeof(count_inst), "%s", __get_parameter_value(buf, tmp));

	lib_resolve_node(L3_FORWARDING, &node);
	count = lib_get_children(node, &children);
	if(children) {
		lib_destroy_children(children);
		children = NULL;
	}

	if(count != atoi(count_inst)) {
		sprintf(buf, "%d", count);
		__set_parameter_value(L3_FORWARDING_NUM, buf);

		lib_resolve_node(L3_FORWARDING_NUM, &node);

		if(node->noc == 1 || node->noc == 2) {
			sprintf(buf, "%s", L3_FORWARDING_NUM);
			add_inform_parameter(buf, 1);
		}
	}
	
	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

static int get_eth_idledisconnecttime(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[16] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char pvc[16] = {0};
#endif
        
	memset(prefix, 0, sizeof(prefix));
#ifdef TR098
	sprintf(prefix, "%s", eth_wanip_prefix_by_path(path, tmp));
#endif
#ifdef TR181
	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));
#endif

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "pppoe_idletime", tmp)));
#else 	/* DSL_ASUSWRT */
	snprintf(buf, sizeof(buf), "%s", tcapi_get_string(convert_prefix_to_pvc(prefix, pvc), "CLOSEIFIDLE", tmp));
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_eth_idledisconnecttime(node_t node, char *arg, char *value)
{
	char prefix[16] = {0}, tmp[32] = {0};
	int res = 0;
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char pvc[16] = {0};
#endif

	memset(prefix, 0, sizeof(prefix));
#ifdef TR098
	sprintf(prefix, "%s", eth_wanip_prefix_by_path(path, tmp));
#endif

#ifdef TR181
	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));
#endif

	if (strlen(prefix) == 0)
		return -1;

#ifdef TR098
#if defined(ASUSWRT) && defined(RTCONFIG_XDSL)
	if (check_dsl_wan_device_by_path(path))
		res = set_nvram("dslx_pppoe_idletime", value);
	else
#endif
#endif

#ifdef ASUSWRT
	res = set_nvram(strcat_r(prefix, "pppoe_idletime", tmp), value);
#else 	/* DSL_ASUSWRT */
	res = set_nvram(convert_prefix_to_pvc(prefix, pvc), "CLOSEIFIDLE", value);
#endif

	return res;
}

static int get_dev_conffile(node_t node, char *arg, char **value) //2016.6.28 sherry
{
    char buf[MAX_NVRAM_SPACE];
    char *name;
    unsigned long count, filelen, i;
    unsigned char rand = 0, temp;
    char rand_t[1024];
    memset(rand_t , 0 , 1024);
    char *value_t;

    nvram_getall(buf, NVRAM_SPACE);

    count = 0;
    for (name = buf; *name; name += strlen(name) + 1)
    {
        count = count + strlen(name) + 1;
    }

    filelen = count + (1024 - count % 1024);
    rand = get_rand() % 30;

     //tr_log(LOG_DEBUG, "%s  file_t=%d,filelen=%ld\n", __FUNCTION__,sizeof(filelen),filelen);
     //tr_log(LOG_DEBUG, "%s  rand_t=%d\n", __FUNCTION__,sizeof(rand));
    value_t = malloc(filelen+10);
    memset(value_t , 0 , filelen+10);//8+count+(filelen-count),+1 long filelen 4

    for (i = 0; i < count; i++)
    {
        if (buf[i] == 0x0)
            buf[i] = 0xfd + get_rand() % 3;
        else
            buf[i] = 0xff - buf[i] + rand;
    }
    //tr_log(LOG_DEBUG, "%s  buf_t=%d\n", __FUNCTION__,strlen(buf));

    for (i = count; i < filelen; i++)
    {
        temp = 0xfd + get_rand() % 3;
        rand_t[i-count] = temp;
    }
    //tr_log(LOG_DEBUG, "%s  rand_t=%d\n", __FUNCTION__,strlen(rand_t));
    snprintf(value_t, filelen+10, "%s%ld%c%s%s", PROFILE_HEADER_NEW, filelen, rand, buf, rand_t);
    //tr_log(LOG_DEBUG, "%s  strlen(value_t)=%d,filelen=%ld\n", __FUNCTION__,strlen(value_t),filelen);

    if( filelen > 32768 )//TODO filelen+10
        *value = strdup("configfile over limit");
    else
        *value = strdup(value_t);

    free(value_t);

   /* int write_len;
    char *file_path="/tmp/Settings_RT-N18U.CFG";
    FILE *fp;
    fp = fopen( file_path, "wb" );

    if( fp == NULL ) {
        tr_log( LOG_ERROR, "Open %s failed: %s", file_path, war_strerror( war_geterror() ) );
        return -1;
    }

    write_len = fwrite( value_t, 1, strlen(value_t) , fp );
    tr_log( LOG_ERROR, "write_len=%d", write_len );

    fflush( fp );
    fclose( fp );*/

    return *value ? 0 : -1;

   /* char *file_path = "/tmp/Settings_RT-N18U.CFG";
        eval("nvram", "save", file_path);

        FILE *fp ;
        fp = fopen( file_path, "rb" );

        if( fp == NULL ) {
            tr_log( LOG_ERROR, "Open %s failed: %s", file_path, war_strerror( war_geterror() ) );
            return -1;
        }

        long int len = tr_file_len(fp);
        tr_log( LOG_ERROR, "len=%ld\n",len );

       // if(len > 32768)
         //   return -1; //TODO

        char *buf = malloc(len+1);
        if( buf == NULL ) {
            tr_log( LOG_ERROR, "Out of memory!" );
        } else {

            memset(buf, 0, len);
            int readlen = fread(buf, 1, len , fp);
            tr_log( LOG_ERROR, "readlen=%ld\n",readlen );
            tr_log( LOG_ERROR, "buf=%ld\n",strlen(buf) );

            *value = strdup(buf);
            free(buf);
        }

        fclose( fp );*/

        return *value ? 0 : -1;

}

static int set_dev_conffile(node_t node, char *arg, char *value)//2016.6.28 sherry
{
    int write_len;
    int len;

    if(value){

        len = strlen(value);

      if( len > 32768 )//TODO
            return -1;

        char *file_path="/tmp/settings_u.prf";
        FILE *fp;
        fp = fopen( file_path, "wb" );

        if( fp == NULL ) {
            tr_log( LOG_ERROR, "Open %s failed: %s", file_path, war_strerror( war_geterror() ) );
            return -1;
        }

        write_len = fwrite( value, 1, len+1 , fp );

        fflush( fp );
        fclose( fp );

        eval("nvram", "restore", file_path);
        nvram_commit();
        unlink(file_path);

        if(write_len != len)
            return -1;
        else
            return 0;
    }
    return -1;
}

static int get_eth_phylinkstatus(node_t node, char *arg, char **value)
{
	char *status = NULL;
	int unit = 0;
	char *path = lib_node2path(node);
#ifdef TR098
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0};

	unit = get_wan_unit_by_path(path);

	/* check prefix */
	memset(prefix, 0, sizeof(prefix));
	eth_wanip_prefix_by_path(path, prefix);
	if (strlen(prefix) == 0)
		unit = -1;
#endif
#ifdef TR181
	unit = ethernet_unit_by_path(path);

	if (unit == -1)	/* something wrong */
		return -1;
#endif
	if (unit == -1)
		status = "Unavailable";
	else if (wanport_status(unit))
		status = "Up";
	else
		status = "Down";

	*value = strdup(status);
	return *value ? 0 : -1;
}

static int get_dev_syslog(node_t node, char *arg, char **value) 
{
	FILE *fp ;
	long int len;
	char *buf, *file_path = "/tmp/syslog.log";

	fp = fopen(file_path, "rb");
	if ( fp == NULL ) {
		tr_log( LOG_ERROR, "Open %s failed: %s", file_path, war_strerror( war_geterror() ) );
		return -1;
	}

	len = tr_file_len(fp);
	if ( len > 32*1024 ) {
		len = 32*1024;
		fseek(fp, -len, SEEK_END);
	}

	buf = malloc(len + 1);
	if ( buf ) {
		len = tr_fread(buf, 1, len, fp);
		if ( len < 0 ) {
			tr_log( LOG_ERROR, "tr_fread %s error:%s", file_path, war_strerror( war_geterror() ) );
			free(buf);
			buf = NULL;
		} else
			buf[len] = '\0';
	} else
		tr_log( LOG_ERROR, "Out of memory!" );
	fclose( fp );

	*value = buf;
	return *value ? 0 : -1;
}

static int set_eth_username(node_t node, char *arg, char *value)
{
	char prefix[16] = {0}, tmp[32] = {0};
	int res = 0;
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char pvc[16] = {0};
#endif

	memset(prefix, 0, sizeof(prefix));
#ifdef TR098
	sprintf(prefix, "%s", eth_wanip_prefix_by_path(path, tmp));
#endif

#ifdef TR181
	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));
#endif

	if (strlen(prefix) == 0)
		return -1;

#ifdef TR098
#if defined(ASUSWRT) && defined(RTCONFIG_XDSL)
	if (check_dsl_wan_device_by_path(path))
		res = set_nvram("dslx_pppoe_username", value);
	else
#endif
#endif

#ifdef ASUSWRT
	res = set_nvram(strcat_r(prefix, "pppoe_username", tmp), value);
#else 	/* DSL_ASUSWRT */
	res = set_nvram(convert_prefix_to_pvc(prefix, pvc), "USERNAME", value);
#endif

	return res;
}

static int get_eth_username(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[16] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char pvc[16] = {0};
#endif
		
	memset(prefix, 0, sizeof(prefix));
#ifdef TR098
	sprintf(prefix, "%s", eth_wanip_prefix_by_path(path, tmp));
#endif

#ifdef TR181
	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));
#endif

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "pppoe_username", tmp)));
#else 	/* DSL_ASUSWRT */
	snprintf(buf, sizeof(buf), "%s", tcapi_get_string(convert_prefix_to_pvc(prefix, pvc), "USERNAME", tmp));
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_eth_password(node_t node, char *arg, char *value)
{
	char prefix[16] = {0}, tmp[32] = {0};
	int res = 0;
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char pvc[16] = {0};
#endif

	memset(prefix, 0, sizeof(prefix));
#ifdef TR098
	sprintf(prefix, "%s", eth_wanip_prefix_by_path(path, tmp));
#endif

#ifdef TR181
	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));
#endif

	if (strlen(prefix) == 0)
		return -1;

#ifdef TR098
#if defined(ASUSWRT) && defined(RTCONFIG_XDSL)
	if (check_dsl_wan_device_by_path(path))
		res = set_nvram("dslx_pppoe_passwd", value);
	else
#endif
#endif

#ifdef ASUSWRT
	res = set_nvram(strcat_r(prefix, "pppoe_passwd", tmp), value);
#else 	/* DSL_ASUSWRT */
	res = set_nvram(convert_prefix_to_pvc(prefix, pvc), "PASSWORD", value);
#endif

	return res;
}

static int get_eth_password(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[16] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char pvc[16] = {0};
#endif

	memset(prefix, 0, sizeof(prefix));
#ifdef TR098
	sprintf(prefix, "%s", eth_wanip_prefix_by_path(path, tmp));
#endif

#ifdef TR181
	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));
#endif

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "pppoe_passwd", tmp)));
#else 	/* DSL_ASUSWRT */
	snprintf(buf, sizeof(buf), "%s", tcapi_get_string(convert_prefix_to_pvc(prefix, pvc), "PASSWORD", tmp));
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_eth_pppoeacname(node_t node, char *arg, char *value)
{
	char prefix[16] = {0}, tmp[32] = {0};
	int res = 0;
	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));
#ifdef TR098
	sprintf(prefix, "%s", eth_wanip_prefix_by_path(path, tmp));
#else
	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));
#endif

	if (strlen(prefix) == 0)
		return -1;

#ifdef TR098
#if defined(ASUSWRT) && defined(RTCONFIG_XDSL)
	if (check_dsl_wan_device_by_path(path))
		res = set_nvram("dslx_pppoe_ac", value);
	else
#endif
#endif

#ifdef ASUSWRT
	res = set_nvram(strcat_r(prefix, "pppoe_ac", tmp), value);
#endif

	return res;
}

static int get_eth_pppoeacname(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[16] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));
#ifdef TR098
	sprintf(prefix, "%s", eth_wanip_prefix_by_path(path, tmp));
#else
	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));
#endif

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "pppoe_ac", tmp)));
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_eth_pppoeservicename(node_t node, char *arg, char *value)
{
	char prefix[16] = {0}, tmp[32] = {0};
	int res = 0;
	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));	
#ifdef TR098
	sprintf(prefix, "%s", eth_wanip_prefix_by_path(path, tmp));
#else
	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));
#endif

	if (strlen(prefix) == 0)
		return -1;

#ifdef TR098
#if defined(ASUSWRT) && defined(RTCONFIG_DSL)
	if (check_dsl_wan_device_by_path(path))
		res = set_nvram("dslx_pppoe_service", value);
	else
#endif
#endif

#ifdef ASUSWRT
	res = set_nvram(strcat_r(prefix, "pppoe_service", tmp), value);
#endif

	return res;
}

static int get_eth_pppoeservicename(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[16] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));	
#ifdef TR098
	sprintf(prefix, "%s", eth_wanip_prefix_by_path(path, tmp));
#else
	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));
#endif

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "pppoe_service", tmp)));
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}



#ifdef DSL_ASUSWRT	/* start of DSL_ASUSWRT */
#ifdef TR098
int convert_wd_inst_to_wan_unit(int wd_inst)
{
	int wan_unit = 0;

	if (wd_inst == IGD_WANDEVICE_DSL_INST) {
#ifdef TCSUPPORT_WAN_ATM
		if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", ADSL_TRANS_MODE)) || !support_adsl_vdsl)
			wan_unit = WAN_ATM_INDEX;
#endif
#ifdef TCSUPPORT_WAN_PTM
		if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", VDSL_TRANS_MODE)) || !support_adsl_vdsl)
			wan_unit = WAN_PTM_INDEX;
#endif
	}
#ifdef TCSUPPORT_WAN_ETHER
	else if (wd_inst == IGD_WANDEVICE_WAN_INST)
		wan_unit = WAN_ETHER_INDEX;
#endif
#ifdef TCSUPPORT_WAN_ETHER
	else if (wd_inst == IGD_WANDEVICE_USB_INST)
		wan_unit = WAN_USB_INDEX;
#endif

	return wan_unit;
}
#endif

#ifdef TR181
int convert_eth_inst_to_wan_unit(int eth_inst)
{
	int wan_unit = 0;

	if (eth_inst == DEVICE_DSL_INST) {
#ifdef TCSUPPORT_WAN_ATM
		if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", ADSL_TRANS_MODE)) || !support_adsl_vdsl)
			wan_unit = WAN_ATM_INDEX;
#endif
#ifdef TCSUPPORT_WAN_PTM
		if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", VDSL_TRANS_MODE)) || !support_adsl_vdsl)
			wan_unit = WAN_PTM_INDEX;
#endif
	}
#ifdef TCSUPPORT_WAN_ETHER
	else if (eth_inst == DEVICE_ETH_WAN_INST)
		wan_unit = WAN_ETHER_INDEX;
#endif
#ifdef TCSUPPORT_WAN_ETHER
	else if (eth_inst == DEVICE_USB_INST)
		wan_unit = WAN_USB_INDEX;
#endif

	return wan_unit;
}
#endif

char *lang_mapping[] = {"", "EN", "BR", "CN", "CZ", 
						"DA", "DE", "ES", "FI", "FR",
						"IT", "MS", "NO", "PL", "RU",
						"SV", "TH", "TR", "TW", "UK"};

int get_lang_index_by_name(char *lang_name)
{
	int result = 0;
	int i = 0;

	tr_log(LOG_DEBUG, "%s - the size of lang_mapping is %d", __FUNCTION__, sizeof(lang_mapping)/sizeof(lang_mapping[0]));

	for (i = 1; i < sizeof(lang_mapping)/sizeof(lang_mapping[0]); i++)
	{
		tr_log(LOG_DEBUG, "%s - %s %s", __FUNCTION__, lang_name, lang_mapping[i]);
		if (!strcasecmp(lang_name, lang_mapping[i])) {
			result = i;
			break;
		}
	}
	tr_log(LOG_DEBUG, "%s - result is %d", __FUNCTION__, result);
	return result;
}

static int get_currentlang(node_t node, char *arg, char ** value)
{
	char buf[4] = {0};

	snprintf(buf, sizeof(buf), "%s", lang_mapping[tcapi_get_int("LanguageSwitch_Entry", "Type")]);
	
	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_currentlang(node_t node, char *arg, char *value)
{
	char buf[4] = {0};

	snprintf(buf, sizeof(buf), "%d", get_lang_index_by_name(value));

	return set_nvram("LanguageSwitch_Entry", "Type", buf);
}

char *convert_prefix_to_pvc(char *prefix, char *pvc)
{
	char buf[16] = {0};

#ifdef TCSUPPORT_WAN_ATM
	snprintf(buf, sizeof(buf), "wan%d_", WAN_ATM_INDEX);
	if (!strcmp(prefix, buf)) sprintf(pvc, "%s%d", WAN_XTM_PREFIX, WAN_ATM_INDEX);
#endif

#ifdef TCSUPPORT_WAN_PTM
	snprintf(buf, sizeof(buf), "wan%d_", WAN_PTM_INDEX);
	if (!strcmp(prefix, buf)) sprintf(pvc, "%s%d", WAN_XTM_PREFIX, WAN_PTM_INDEX);
#endif

#ifdef TCSUPPORT_WAN_ETHER
	snprintf(buf, sizeof(buf), "wan%d_", WAN_ETHER_INDEX);
	if (!strcmp(prefix, buf)) sprintf(pvc, "%s%d", WAN_XTM_PREFIX, WAN_ETHER_INDEX);
#endif

#ifdef RTCONFIG_USB_MODEM
	snprintf(buf, sizeof(buf), "wan%d_", WAN_USB_INDEX);
	if (!strcmp(prefix, buf)) sprintf(pvc, "%s%d", WAN_XTM_PREFIX, WAN_USB_INDEX);
#endif

	return pvc;
}

int convert_prefix_to_wan_unit(char *prefix)
{
	char buf[16] = {0};
	int unit = 0;

#ifdef TCSUPPORT_WAN_ATM
	snprintf(buf, sizeof(buf), "wan%d_", WAN_ATM_INDEX);
	if (!strcmp(prefix, buf)) unit = WAN_ATM_INDEX;
#endif

#ifdef TCSUPPORT_WAN_PTM
	snprintf(buf, sizeof(buf), "wan%d_", WAN_PTM_INDEX);
	if (!strcmp(prefix, buf)) unit = WAN_PTM_INDEX;
#endif

#ifdef TCSUPPORT_WAN_ETHER
	snprintf(buf, sizeof(buf), "wan%d_", WAN_ETHER_INDEX);
	if (!strcmp(prefix, buf)) unit = WAN_ETHER_INDEX;
#endif

#ifdef RTCONFIG_USB_MODEM
	snprintf(buf, sizeof(buf), "wan%d_", WAN_USB_INDEX);
	if (!strcmp(prefix, buf)) unit = WAN_USB_INDEX;
#endif

	return unit;
}
#endif 	/* DSL_ASUSWRT */



int write_port_mapping_to_nvram(char *path, int skip_index)
{
	node_t *children = NULL;
	node_t node;
	int count = 0;
	char buf[256] = {0}, tmp[64] = {0}, vts_rulelist[2048] = {0};

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	if(lib_resolve_node(path, &node) == 0) {
		memset(vts_rulelist, 0, sizeof(vts_rulelist));

		count = lib_get_children(node, &children);
		while(count > 0) {
			char name[16];
			count--;

			if(lib_get_property(children[count], "name", name) == 0) {
		    		if(string_is_digits(name) == 1 && (skip_index != atoi(name))) {
					char ext_port[32] = {0}, ext_port_end[8] = {0}, in_port[8] = {0}, protocol[8] = {0}, src_addr[64] = {0}, in_client[64] = {0}, description[32] = {0};
					char vts_rule[64] = {0};

					memset(vts_rule, 0, sizeof(vts_rule));
#ifdef ASUSWRT				
					/* get the data of RemoteHost */
					snprintf(buf, sizeof(buf), "%s.%s.RemoteHost", path, name);
					sprintf(src_addr, "%s", __get_parameter_value(buf, tmp));

					/* get the data of ExternalPort */
					snprintf(buf, sizeof(buf), "%s.%s.ExternalPort", path, name);
					sprintf(ext_port, "%s", __get_parameter_value(buf, tmp));
#else 	/* DSL_ASUSWRT */
					char remote_host_str[32] = {0}, ext_port_str[8] = {0};

					memset(remote_host_str, 0, sizeof(remote_host_str));
					memset(ext_port_str, 0, sizeof(ext_port_str));

					/* get the data of RemoteHost */
					snprintf(buf, sizeof(buf), "%s.%s.RemoteHost", path, name);
					sprintf(remote_host_str, "%s", __get_parameter_value(buf, tmp));

					/* get the data of ExternalPort */
					snprintf(buf, sizeof(buf), "%s.%s.ExternalPort", path, name);
					sprintf(ext_port_str, "%s", __get_parameter_value(buf, tmp));

					if (strlen(remote_host_str))
						snprintf(ext_port, sizeof(ext_port), "%s@%s", remote_host_str, ext_port_str);
					else
						snprintf(ext_port, sizeof(ext_port), "%s", ext_port_str);
#endif						
					/* get the data of ExternalPortEndRange */
					snprintf(buf, sizeof(buf), "%s.%s.ExternalPortEndRange", path, name);
					sprintf(ext_port_end, "%s", __get_parameter_value(buf, tmp));

					/* get the data of InternalPort */
					snprintf(buf, sizeof(buf), "%s.%s.InternalPort", path, name);
					sprintf(in_port, "%s", __get_parameter_value(buf, tmp));
						
					/* get the data of InternalClient */
					snprintf(buf, sizeof(buf), "%s.%s.InternalClient", path, name);
					sprintf(in_client, "%s", __get_parameter_value(buf, tmp));
#ifdef TR098
					/* get the data of PortMappingProtocol */
					snprintf(buf, sizeof(buf), "%s.%s.PortMappingProtocol", path, name);
					sprintf(protocol, "%s", __get_parameter_value(buf, tmp));

					/* get the data of PortMappingDescription */
					snprintf(buf, sizeof(buf), "%s.%s.PortMappingDescription", path, name);
					sprintf(description, "%s", __get_parameter_value(buf, tmp));
#endif
#ifdef TR181
					/* get the data of PortMappingProtocol */
					snprintf(buf, sizeof(buf), "%s.%s.Protocol", path, name);
					sprintf(protocol, "%s", __get_parameter_value(buf, tmp));

					/* get the data of PortMappingDescription */
					snprintf(buf, sizeof(buf), "%s.%s.Description", path, name);
					sprintf(description, "%s", __get_parameter_value(buf, tmp));
#endif
					if(!strcmp(ext_port_end, "0"))
						snprintf(vts_rule, sizeof(buf), "<%s>%s>%s>%s>%s>%s",
								description, ext_port, in_client, in_port, protocol, src_addr);
					else
						snprintf(vts_rule, sizeof(buf), "<%s>%s:%s>%s>%s>%s>%s",
								description, ext_port, ext_port_end, in_client, in_port, protocol, src_addr);
					
					if(vts_rule[0] != '\0')
						strcat(vts_rulelist, vts_rule);
		    		}
			}
		}

		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}

		//set_nvram("vts_rulelist", vts_rulelist);
	}

#ifdef ASUSWRT
	set_nvram("vts_rulelist", vts_rulelist);
#else
	set_nvram(VIRSERVER_NODE, "VirServer_RuleList", vts_rulelist);
#endif

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

static int add_eth_portmapping(node_t node, char *arg, int nin)
{
#ifdef TR098
	char *path = lib_node2path(node);
	char buf[256] = {0}, prefix[sizeof("wanXXXXXXXXXX_")] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *wan_proto = NULL;
#else 	/* DSL_ASUSWRT */
	char wan_proto[8] = {0};
#endif
	char full_path[256] = {0};
	//int inst_num = 0;
	int unit = 0;

	snprintf(full_path, sizeof(full_path), "%s", path);
	tr_log(LOG_DEBUG, "%s - full_path %s", __FUNCTION__, full_path);

	for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit ++) {
		int wd_inst = 0;

#ifdef ASUSWRT
		wd_inst = get_wd_inst_by_wan_unit(unit);
#else 	/* DSL_ASUSWRT */
		if (unit == 0)
			wd_inst = pri_wan_inst;
		else if (unit == 1)
			wd_inst = sec_wan_inst;
#endif
		
#ifdef RTCONFIG_XDSL
#ifdef ASUSWRT
		if (wd_inst == IGD_WANDEVICE_WAN_INST)
#else 	/* DSL_ASUSWRT */
		if (wd_inst == IGD_WANDEVICE_DSL_INST)			
#endif
		{
			add_del_xdsl_portmapping(unit, wd_inst, full_path, nin, PM_ADD);
		}
		else
#endif		
		if (wd_inst) {

#ifdef ASUSWRT
			snprintf(prefix, sizeof(prefix), "wan%d_", unit);
			wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
#else 	/* DSL_ASUSWRT */
			snprintf(prefix, sizeof(prefix), "wan%d_", convert_wd_inst_to_wan_unit(wd_inst));
			tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);
#endif

			snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnection", IGD_WANDEVICE, wd_inst);
			add_portmapping_entry(full_path, nin, buf);

			/* Update wan_proto as PPPoE,PPTP,L2TP */
			if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp")) {
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection", IGD_WANDEVICE, wd_inst);
				add_portmapping_entry(full_path, nin, buf);
			}	
		}
	}
#endif

#ifdef ASUSWRT
	set_nvram("vts_enable_x", "1");	/* enable vts */
#else 	/* DSL_ASUSWRT */
	set_nvram(VIRSERVER_NODE, "VirServer_Enable", "1");
#endif

	return 0;
}

static int del_eth_portmapping(node_t node, char *arg, int nin)
{
	char *path = lib_node2path(node);
#ifdef TR098
	char buf[256] = {0}, prefix[sizeof("wanXXXXXXXXXX_")] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *wan_proto = NULL;
#else 	/* DSL_ASUSWRT */
	char wan_proto[8] = {0};
#endif
	char full_path[256];
	int unit = 0;
	//int inst_num = 0;

	snprintf(full_path, sizeof(full_path), "%s%d", path, nin);
	tr_log(LOG_DEBUG, "%s - full_path (%s)", __FUNCTION__, full_path);
#endif

	path[strlen(path) - 1] = '\0';
	tr_log(LOG_DEBUG, "%s - path (%s) %s.%d", __FUNCTION__, path, arg, nin);

	write_port_mapping_to_nvram(path, nin);
	/* nned to save config aftet write_port_mapping_to_nvram */
#ifdef ASUSWRT
	nvram_commit();
#else 	/* DSL_ASUSWRT */
	tcapi_save();
#endif

#ifdef TR098
	for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit ++) {
		int wd_inst = 0;
#ifdef ASUSWRT
		wd_inst = get_wd_inst_by_wan_unit(unit);
#else 	/* DSL_ASUSWRT */
		if (unit == 0)
			wd_inst = pri_wan_inst;
		else if (unit == 1)
			wd_inst = sec_wan_inst;
#endif

#ifdef RTCONFIG_XDSL
#ifdef ASUSWRT
		if (wd_inst == IGD_WANDEVICE_WAN_INST)
#else 	/* DSL_ASUSWRT */
		if (wd_inst == IGD_WANDEVICE_DSL_INST)
#endif
		{
			add_del_xdsl_portmapping(unit, wd_inst, full_path, nin, PM_DEL);
		}
		else
#endif		
		if (wd_inst) {
#ifdef ASUSWRT
			snprintf(prefix, sizeof(prefix), "wan%d_", unit);
			wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
#else 	/* DSL_ASUSWRT */
			snprintf(prefix, sizeof(prefix), "wan%d_", convert_wd_inst_to_wan_unit(wd_inst));
			tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);
#endif

			snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnection", IGD_WANDEVICE, wd_inst);
			del_portmapping_entry(full_path, nin, buf);

			/* Update wan_proto as PPPoE,PPTP,L2TP */
			if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp")) {
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection", IGD_WANDEVICE, wd_inst);
				del_portmapping_entry(full_path, nin, buf);	
			}	
		}
	}

	/* Reasign instance name of port mapping */
	//reasign_instance_name(node, nin, 0);
#endif

	return 0;
}

static int set_port_mapping(node_t node, char *arg, char *value)
{
	//char *path = lib_node2path(node->parent);
	char *path = NULL;
	int res = 0;
#ifdef TR098
	char *node_path = lib_node2path(node);
	char path_str[256] = {0}, tmp[32] = {0}, prefix[sizeof("wanXXXXXXXXXX_")] = {0};
#ifdef ASUSWRT
	char *wan_proto = NULL;
#else 	/* DSL_ASUSWRT */
	char wan_proto[8] = {0};
#endif
	char buf[256] = {0}, *tok = NULL;
	char field[64] = {0};
	int nin = 0, unit = 0, inst_num = 0;

	snprintf(path_str, sizeof(path_str), "%s", node_path);
#endif

	path = lib_node2path(node->parent->parent);
	path[strlen(path) - 1] = '\0';

	tr_log( LOG_DEBUG, "%s - %s", __FUNCTION__, path);

	res = write_port_mapping_to_nvram(path, 0);

#ifdef TR098
	//tr_log(LOG_DEBUG, "The path: %s", path_str);
	/* Get instance number and field */
	sprintf(buf, ".PortMapping.");
	tok = strstr(path_str, buf);
	if (tok) {
		tok = tok + strlen(buf);
		sscanf(tok, "%u.%s.", &nin, field);
	}
	tr_log(LOG_DEBUG, "nin: %d  field: %s", nin, field);

	/* reset other portmapping entry */
	for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit ++) {
		int wd_inst = 0;
#ifdef ASUSWRT
			wd_inst = get_wd_inst_by_wan_unit(unit);
#else 	/* DSL_ASUSWRT */
		if (unit == 0)
			wd_inst = pri_wan_inst;
		else if (unit == 1)
			wd_inst = sec_wan_inst;
#endif

#ifdef RTCONFIG_XDSL
#ifdef ASUSWRT
		if (wd_inst == IGD_WANDEVICE_WAN_INST)
#else 	/* DSL_ASUSWRT */
		if (wd_inst == IGD_WANDEVICE_DSL_INST)
#endif
		{
			set_xdsl_portmapping(unit, wd_inst, nin, field, value);
		}
		else
#endif		
		if (wd_inst) {
#ifdef ASUSWRT
			snprintf(prefix, sizeof(prefix), "wan%d_", unit);
			wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
#else 	/* DSL_ASUSWRT */
			snprintf(prefix, sizeof(prefix), "wan%d_", convert_wd_inst_to_wan_unit(wd_inst));
			tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);
#endif
			snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnection", IGD_WANDEVICE, wd_inst);
			inst_num = get_inst_num_by_path(buf);

			if (inst_num) {
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnection.%d.PortMapping.%d.%s", IGD_WANDEVICE, wd_inst, inst_num, nin, field);
				tr_log(LOG_DEBUG, "The path for set: %s", buf);
				update_flag = 1;
				__set_parameter_value(buf, value);
				update_flag = 0;
			}

			if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp")) {	/* PPPoE,PPTP,L2TP */
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection", IGD_WANDEVICE, wd_inst);
				inst_num = get_inst_num_by_path(buf);

				if (inst_num) {
					snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection.%d.PortMapping.%d.%s", IGD_WANDEVICE, wd_inst, inst_num, nin, field);
					tr_log(LOG_DEBUG, "The path for set: %s", buf);
					update_flag = 1;
					__set_parameter_value(buf, value);
					update_flag = 0;
				}
			}
		}		
	}

#endif

	return res;
}

int add_portmapping_instance(char *path, char *src_addr, char *description, char *ext_port, char *in_client, char *in_port, char *protocol)
{
	char buf[128] = {0};
	int i = 0;

	tr_log(LOG_DEBUG, "path: %s, desc: %s, src_addr %s, ext_port: %s, in_client: %s, in_port: %s, protocol: %s", 
				path, description, src_addr, ext_port, in_client, in_port, protocol);

	/* add an instance of dhcp static list */
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "%s.", path);
	if((i = add_object(buf, strlen(buf))) < 9000) {
		/* set ExternalPort, ExternalPortEndRange, InternalPort, PortMappingProtocol, InternalClient, PortMappingDescription */
		char *ptr = NULL;

		if((ptr = strchr(ext_port, ':')) != NULL) {
			*ptr = 0x0;
			ptr++;
			snprintf(buf, sizeof(buf), "%s.%d.ExternalPortEndRange", path, i);		
			__set_parameter_value(buf, ptr);
		
		} else {
			snprintf(buf, sizeof(buf), "%s.%d.ExternalPortEndRange", path, i);		
			__set_parameter_value(buf, "0");
		}

#ifdef ASUSWRT
		snprintf(buf, sizeof(buf), "%s.%d.RemoteHost", path, i);
		__set_parameter_value(buf, src_addr);

		snprintf(buf, sizeof(buf), "%s.%d.ExternalPort", path, i);
		__set_parameter_value(buf, ext_port);
#else 	/* DSL_ASUSWRT */
		char tmp_buf_1[32] = {0}, tmp_buf_2[32] = {0}, tmp_buf_3[32] = {0};
		char *target_ptr = NULL, *remote_host_ptr = NULL, *ext_port_ptr = NULL;

		memset(tmp_buf_1, 0, sizeof(tmp_buf_1));
		memset(tmp_buf_2, 0, sizeof(tmp_buf_2));
		memset(tmp_buf_3, 0, sizeof(tmp_buf_3));

		snprintf(tmp_buf_1, sizeof(tmp_buf_1), "%s", ext_port);
		target_ptr = strstr(tmp_buf_1, "@");

		if (target_ptr == NULL){
			ext_port_ptr = tmp_buf_1;
			remote_host_ptr = tmp_buf_2;
		}
		else
		{
			strncpy(tmp_buf_2, tmp_buf_1, target_ptr - tmp_buf_1);
			sprintf(tmp_buf_3, "%s", (target_ptr+1));
			remote_host_ptr = tmp_buf_2;
			ext_port_ptr = tmp_buf_3;
		}

		snprintf(buf, sizeof(buf), "%s.%d.RemoteHost", path, i);
		__set_parameter_value(buf, remote_host_ptr);

		snprintf(buf, sizeof(buf), "%s.%d.ExternalPort", path, i);
		__set_parameter_value(buf, ext_port_ptr);
#endif

		snprintf(buf, sizeof(buf), "%s.%d.InternalPort", path, i);		
		__set_parameter_value(buf, in_port);

		snprintf(buf, sizeof(buf), "%s.%d.InternalClient", path, i);		
		__set_parameter_value(buf, in_client);
#ifdef TR098
		snprintf(buf, sizeof(buf), "%s.%d.PortMappingProtocol", path, i);		
		__set_parameter_value(buf, protocol);

		snprintf(buf, sizeof(buf), "%s.%d.PortMappingDescription", path, i);		
		__set_parameter_value(buf, description);
#endif
#ifdef TR181
		snprintf(buf, sizeof(buf), "%s.%d.Protocol", path, i);		
		__set_parameter_value(buf, protocol);

		snprintf(buf, sizeof(buf), "%s.%d.Description", path, i);		
		__set_parameter_value(buf, description);
#endif
	}
	else
		return -1;

	return 0;
}

int compare_portmapping_instance(char *pm_buf, char *description, char *src_addr, char *ext_port, char *in_client, char *in_port, char *protocol)
{
	node_t *children = NULL;
	node_t node;
	int count = 0, need_add = 1;
	char tmp[64] = {0};
	char buf[128] = {0};

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	if(lib_resolve_node(pm_buf, &node) == 0) {
		count = lib_get_children(node, &children);
		
		while(count > 0) {
			char name[16] = {0};
			count--;

			if(lib_get_property(children[count], "name", name) == 0) {
				if(string_is_digits(name) == 1) {
					char ext_port_inst[32] = {0}, ext_port_end_inst[32] = {0}, in_port_inst[32] = {0}, 
						proto_inst[8] = {0}, in_client_inst[32] = {0}, desc_inst[32] = {0},
						src_addr_inst[32] = {0};

#ifdef DSL_ASUSWRT
					char remote_host_str[32] = {0}, ext_port_str[32] = {0};

					snprintf(buf, sizeof(buf), "%s.%s.RemoteHost", pm_buf, name);
					snprintf(remote_host_str, sizeof(remote_host_str), "%s", __get_parameter_value(buf, tmp));	

					/* get the data of ExternalPort */
					snprintf(buf, sizeof(buf), "%s.%s.ExternalPort", pm_buf, name);
					snprintf(ext_port_str, sizeof(ext_port_str), "%s", __get_parameter_value(buf, tmp));

					if (strlen(remote_host_str))
						snprintf(ext_port_inst, sizeof(ext_port_inst), "%s@%s", remote_host_str, ext_port_str);
					else
						snprintf(ext_port_inst, sizeof(ext_port_inst), "%s", ext_port_str);
#else /* ASUSWRT */
					/* get the data of RemoteHost */
					snprintf(buf, sizeof(buf), "%s.%s.RemoteHost", pm_buf, name);
					snprintf(src_addr_inst, sizeof(src_addr_inst), "%s", __get_parameter_value(buf, tmp));

					/* get the data of ExternalPort */
					snprintf(buf, sizeof(buf), "%s.%s.ExternalPort", pm_buf, name);
					snprintf(ext_port_inst, sizeof(ext_port_inst), "%s", __get_parameter_value(buf, tmp));
#endif

					/* get the data of ExternalPortEndRange */
					snprintf(buf, sizeof(buf), "%s.%s.ExternalPortEndRange", pm_buf, name);
					snprintf(ext_port_end_inst, sizeof(ext_port_end_inst), "%s", __get_parameter_value(buf, tmp));

					/* get the data of InternalPort */
					snprintf(buf, sizeof(buf), "%s.%s.InternalPort", pm_buf, name);
					snprintf(in_port_inst, sizeof(in_port_inst), "%s", __get_parameter_value(buf, tmp));
		
					/* get the data of InternalClient */
					snprintf(buf, sizeof(buf), "%s.%s.InternalClient", pm_buf, name);
					snprintf(in_client_inst, sizeof(in_client_inst), "%s", __get_parameter_value(buf, tmp));

					/* get the data of PortMappingProtocol */
#ifdef TR098					
					snprintf(buf, sizeof(buf), "%s.%s.PortMappingProtocol", pm_buf, name);
#endif
#ifdef TR181
					snprintf(buf, sizeof(buf), "%s.%s.Protocol", pm_buf, name);
#endif					
					snprintf(proto_inst, sizeof(proto_inst), "%s", __get_parameter_value(buf, tmp));

					/* get the data of PortMappingDescription */
#ifdef TR098
					snprintf(buf, sizeof(buf), "%s.%s.PortMappingDescription", pm_buf, name);
#endif
#ifdef TR181
					snprintf(buf, sizeof(buf), "%s.%s.Description", pm_buf, name);
#endif
					snprintf(desc_inst, sizeof(desc_inst), "%s", __get_parameter_value(buf, tmp));

					if(strcmp(ext_port_end_inst, "0"))
						snprintf(ext_port_inst, sizeof(ext_port_inst), "%s:%s", ext_port_inst, ext_port_end_inst);

					if (!strcmp(ext_port, ext_port_inst) && !strcmp(in_port, in_port_inst) && 
					    !strcmp(protocol, proto_inst) && !strcmp(in_client, in_client_inst) && 
					    !strcmp(description, desc_inst) && !strcmp(src_addr, src_addr_inst)) {
						need_add = 0;
						break;
					}
				}
			}
		}

		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}
	}

	if(need_add)
		add_portmapping_instance(pm_buf, description, src_addr, ext_port, in_client, in_port, protocol);

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

int compare_portmapping_nvram(char *path, char *ext_port_inst, char *in_port_inst, 
	char *proto_inst, char *src_addr_inst, char *in_client_inst, char *desc_inst, int inst_index)
{
	char buf[128] = {0};
	char *nv, *nvp, *b;
	char *ext_port, *in_port, *protocol, *src_addr, *in_client, *description;
	int cnt, need_del = 1;
#ifdef DSL_ASUSWRT
	char rulelist_tmp[2048] = {0};
#endif

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

#ifdef ASUSWRT
	nv = nvp = strdup(nvram_safe_get("vts_rulelist"));
#else 	/* DSL_ASUSWRT */
	nv = nvp = strdup(tcapi_get_string(VIRSERVER_NODE, "VirServer_RuleList", rulelist_tmp));
#endif
	if (nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			if ((cnt = vstrsep(b, ">", &description, &ext_port, &in_client, &in_port, &protocol, &src_addr)) < 5)
				continue;
			else if (cnt < 6)
				src_addr = "";

			if (!strcmp(ext_port, ext_port_inst) && !strcmp(in_port, in_port_inst) && 
			    !strcmp(protocol, proto_inst) && !strcmp(in_client, in_client_inst) &&
			    !strcmp(description, desc_inst) && !strcmp(src_addr, src_addr_inst)) {
				need_del = 0;
				break;
			}
		}
		free(nv);
	}

	if(need_del) {
		node_t node;

		sprintf(buf, "%s.%d.", path, inst_index);
		tr_log(LOG_DEBUG, "The path for delete: %s", buf);
		delete_object(buf, strlen(buf));

		/* Reasign instance name of port mapping */
		snprintf(buf, sizeof(buf), "%s.", path);	
		if(lib_resolve_node(buf, &node) == 0)
			reasign_instance_name(node, inst_index, 1);
	}

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

int update_port_mapping()
{
	char buf[128] = {0};
	char *nv = NULL, *nvp = NULL, *b = NULL;
	char *ext_port = NULL, *in_port = NULL, *protocol = NULL, *src_addr = NULL, *in_client = NULL, *description = NULL;
	node_t node, *children = NULL;
	char tmp[64] = {0}, pm_buf[128] = {0};
	int cnt, count = 0;
#ifdef TR098
	int inst_num = 0, unit = 0;
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0};
#ifdef ASUSWRT
	char *wan_proto = NULL;
#else 	/* DSL_ASUSWRT */
	char wan_proto[8] = {0};
#endif
	
#endif

#ifdef TR181
	char count_inst[8] = {0};
	snprintf(pm_buf, sizeof(pm_buf), "%s.PortMapping", DEVICE_NAT);
#endif


#ifdef DSL_ASUSWRT
	char rulelist_tmp[2048] = {0};
#endif

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	/* compare protmapping on tree's node */
#ifdef ASUSWRT
	nv = nvp = strdup(nvram_safe_get("vts_rulelist"));
#else 	/* DSL_ASUSWRT */
	nv = nvp = strdup(tcapi_get_string(VIRSERVER_NODE, "VirServer_RuleList", rulelist_tmp));
#endif
	if (nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			if ((cnt = vstrsep(b, ">", &description, &ext_port, &in_client, &in_port, &protocol, &src_addr)) < 5)
				continue;
			else if (cnt < 6)
				src_addr = "";

#ifdef TR098
			for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit ++) {
				int wd_inst = 0;
#ifdef ASUSWRT
				wd_inst = get_wd_inst_by_wan_unit(unit);
#else 	/* DSL_ASUSWRT */
				if (unit == 0)
					wd_inst = pri_wan_inst;
				else if (unit == 1)
					wd_inst = sec_wan_inst;
#endif

#ifdef RTCONFIG_XDSL
#ifdef ASUSWRT
				if (wd_inst == IGD_WANDEVICE_WAN_INST) 
#else 	/* DSL_ASUSWRT */
				if (wd_inst == IGD_WANDEVICE_DSL_INST) 
#endif
				{
					xdsl_compare_portmapping_instance(unit, wd_inst, description, src_addr, ext_port, in_client, in_port, protocol);
				}
				else
#endif
				if (wd_inst) {
#ifdef ASUSWRT
					snprintf(prefix, sizeof(prefix), "wan%d_", unit);
					wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
#else 	/* DSL_ASUSWRT */
					snprintf(prefix, sizeof(prefix), "wan%d_", convert_wd_inst_to_wan_unit(wd_inst));
					tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);
#endif
					snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnection", IGD_WANDEVICE, wd_inst);
					inst_num = get_inst_num_by_path(buf);

					if (inst_num) {
						snprintf(pm_buf, sizeof(pm_buf), "%s.%d.WANConnectionDevice.1.WANIPConnection.%d.PortMapping", IGD_WANDEVICE, wd_inst, inst_num);
						compare_portmapping_instance(pm_buf, description, src_addr, ext_port, in_client, in_port, protocol);
					}

					if (!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp")) {
						snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection", IGD_WANDEVICE, wd_inst);
						inst_num = get_inst_num_by_path(buf);

						if (inst_num) {
							snprintf(pm_buf, sizeof(pm_buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection.%d.PortMapping", IGD_WANDEVICE, wd_inst, inst_num);
							compare_portmapping_instance(pm_buf, description, src_addr, ext_port, in_client, in_port, protocol);
						}
					}	
				}
			}
#endif

#ifdef TR181
			compare_portmapping_instance(pm_buf, description, src_addr, ext_port, in_client, in_port, protocol);
#endif
		}
		free(nv);
	}

#ifdef TR098
	/* pick up one as the path of basic port mapping */
	for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit ++) {
		int wd_inst = 0;

#ifdef ASUSWRT
		wd_inst = get_wd_inst_by_wan_unit(unit);
#else 	/* DSL_ASUSWRT */
		if (unit == 0)
			wd_inst = pri_wan_inst;
		else if (unit == 1)
			wd_inst = sec_wan_inst;
#endif

#ifdef RTCONFIG_XDSL
#ifdef ASUSWRT
		if (wd_inst == IGD_WANDEVICE_WAN_INST)
#else 	/* DSL_ASUSWRT */
		if (wd_inst == IGD_WANDEVICE_DSL_INST)
#endif
		{
			memset(pm_buf, 0, sizeof(pm_buf));
			xdsl_pick_portmapping_instance(unit, wd_inst, pm_buf);
			if (strlen(pm_buf))	/* pick up one */
				break;
		}
		else
#endif
		if (wd_inst) {
#ifdef ASUSWRT
			snprintf(prefix, sizeof(prefix), "wan%d_", unit);
			wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
#else 	/* DSL_ASUSWRT */
			snprintf(prefix, sizeof(prefix), "wan%d_", convert_wd_inst_to_wan_unit(wd_inst));
			tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);
#endif
			snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnection", IGD_WANDEVICE, wd_inst);
			inst_num = get_inst_num_by_path(buf);

			if (inst_num) {
				snprintf(pm_buf, sizeof(pm_buf), "%s.%d.WANConnectionDevice.1.WANIPConnection.%d.PortMapping", IGD_WANDEVICE, wd_inst, inst_num);
				break;
			}

			if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp")) {
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection", IGD_WANDEVICE, wd_inst);
				inst_num = get_inst_num_by_path(buf);

				if (inst_num) {
					snprintf(pm_buf, sizeof(pm_buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection.%d.PortMapping", IGD_WANDEVICE, wd_inst, inst_num);
					break;
				}
			}	
		}
	}	

	if (strlen(pm_buf) > 0)
#endif

	{
		/* compare protmapping on nvram */
		if(lib_resolve_node(pm_buf, &node) == 0) {
			count = lib_get_children(node, &children);
			
			while(count > 0) {
				char name[16] = {0};
				count--;

				if(lib_get_property(children[count], "name", name) == 0) {
					if(string_is_digits(name) == 1) {
						char ext_port_inst[32] = {0}, ext_port_end_inst[32] = {0}, in_port_inst[32] = {0}, proto_inst[8] = {0}, 
							in_client_inst[32] = {0}, desc_inst[32] = {0},
							src_addr_inst[32] = {0};

#ifdef ASUSWRT						
						/* get the data of RemoteHost */
						snprintf(buf, sizeof(buf), "%s.%s.RemoteHost", pm_buf, name);
						snprintf(src_addr_inst, sizeof(src_addr_inst), "%s", __get_parameter_value(buf, tmp));

						/* get the data of ExternalPort */
						snprintf(buf, sizeof(buf), "%s.%s.ExternalPort", pm_buf, name);
						snprintf(ext_port_inst, sizeof(ext_port_inst), "%s", __get_parameter_value(buf, tmp));
#else 	/* DSL_ASUSWRT */
						char remote_host_tmp[32] = {0}, ext_port_tmp[32] = {0};

						memset(remote_host_tmp, 0, sizeof(remote_host_tmp));
						memset(ext_port_tmp, 0, sizeof(ext_port_tmp));

						/* get the data of RemoteHost */
						snprintf(buf, sizeof(buf), "%s.%s.RemoteHost", pm_buf, name);
						snprintf(remote_host_tmp, sizeof(remote_host_tmp), "%s", __get_parameter_value(buf, tmp));

						/* get the data of ExternalPort */
						snprintf(buf, sizeof(buf), "%s.%s.ExternalPort", pm_buf, name);
						snprintf(ext_port_tmp, sizeof(ext_port_tmp), "%s", __get_parameter_value(buf, tmp));

						if (strlen(remote_host_tmp))
							snprintf(ext_port_inst, sizeof(ext_port_inst), "%s@%s", remote_host_tmp, ext_port_tmp);
						else
							snprintf(ext_port_inst, sizeof(ext_port_inst), "%s", ext_port_tmp);
#endif		

						/* get the data of ExternalPortEndRange */
						snprintf(buf, sizeof(buf), "%s.%s.ExternalPortEndRange", pm_buf, name);
						snprintf(ext_port_end_inst, sizeof(ext_port_end_inst), "%s", __get_parameter_value(buf, tmp));

						/* get the data of InternalPort */
						snprintf(buf, sizeof(buf), "%s.%s.InternalPort", pm_buf, name);
						snprintf(in_port_inst, sizeof(in_port_inst), "%s", __get_parameter_value(buf, tmp));
			
						/* get the data of InternalClient */
						snprintf(buf, sizeof(buf), "%s.%s.InternalClient", pm_buf, name);
						snprintf(in_client_inst, sizeof(in_client_inst), "%s", __get_parameter_value(buf, tmp));
#ifdef TR098
						/* get the data of PortMappingProtocol */
						snprintf(buf, sizeof(buf), "%s.%s.PortMappingProtocol", pm_buf, name);
						snprintf(proto_inst, sizeof(proto_inst), "%s", __get_parameter_value(buf, tmp));

						/* get the data of PortMappingDescription */
						snprintf(buf, sizeof(buf), "%s.%s.PortMappingDescription", pm_buf, name);
						snprintf(desc_inst, sizeof(desc_inst), "%s", __get_parameter_value(buf, tmp));
#endif

#ifdef TR181
						/* get the data of PortMappingProtocol */
						snprintf(buf, sizeof(buf), "%s.%s.Protocol", pm_buf, name);
						snprintf(proto_inst, sizeof(proto_inst), "%s", __get_parameter_value(buf, tmp));

						/* get the data of PortMappingDescription */
						snprintf(buf, sizeof(buf), "%s.%s.Description", pm_buf, name);
						snprintf(desc_inst, sizeof(desc_inst), "%s", __get_parameter_value(buf, tmp));
#endif
						if(strcmp(ext_port_end_inst, "0"))
							snprintf(ext_port_inst, sizeof(ext_port_inst), "%s:%s", ext_port_inst, ext_port_end_inst);

						//compare_portmapping_nvram(pm_buf, ext_port_inst, in_port_inst, proto_inst, src_addr_inst, in_client_inst, desc_inst, atoi(name));
#ifdef TR098
						for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit ++) {
							int wd_inst = 0;

#ifdef ASUSWRT
							wd_inst = get_wd_inst_by_wan_unit(unit);
#else 	/* DSL_ASUSWRT */
							if (unit == 0)
								wd_inst = pri_wan_inst;
							else if (unit == 1)
								wd_inst = sec_wan_inst;
#endif

#ifdef RTCONFIG_XDSL
#ifdef ASUSWRT
							if (wd_inst == IGD_WANDEVICE_WAN_INST)
#else 	/* DSL_ASUSWRT */
							if (wd_inst == IGD_WANDEVICE_DSL_INST)
#endif
							{
								xdsl_compare_portmapping_nvram(unit, wd_inst, ext_port_inst, in_port_inst, proto_inst, src_addr_inst, in_client_inst, desc_inst, atoi(name));
							}
							else
#endif							
							if (wd_inst) {
#ifdef ASUSWRT
								snprintf(prefix, sizeof(prefix), "wan%d_", unit);
								wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
#else 	/* DSL_ASUSWRT */
								snprintf(prefix, sizeof(prefix), "wan%d_", convert_wd_inst_to_wan_unit(wd_inst));
								tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);
#endif
								snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnection", IGD_WANDEVICE, wd_inst);
								inst_num = get_inst_num_by_path(buf);

								if (inst_num) {
									snprintf(pm_buf, sizeof(pm_buf), "%s.%d.WANConnectionDevice.1.WANIPConnection.%d.PortMapping", IGD_WANDEVICE, wd_inst, inst_num);
									compare_portmapping_nvram(pm_buf, ext_port_inst, in_port_inst, proto_inst, src_addr_inst, in_client_inst, desc_inst, atoi(name));
								}

								if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp")) {
									snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection", IGD_WANDEVICE, wd_inst);
									inst_num = get_inst_num_by_path(buf);

									if (inst_num) {
										snprintf(pm_buf, sizeof(pm_buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection.%d.PortMapping", IGD_WANDEVICE, wd_inst, inst_num);
										compare_portmapping_nvram(pm_buf, ext_port_inst, in_port_inst, proto_inst, src_addr_inst, in_client_inst, desc_inst, atoi(name));
									}
								}	
							}
						}
#endif

#ifdef TR181
						compare_portmapping_nvram(pm_buf, ext_port_inst, in_port_inst, proto_inst, src_addr_inst, in_client_inst, desc_inst, atoi(name));
#endif
					}
				}
			}

			if(children) {
				lib_destroy_children(children);
				children = NULL;
			}
		}
	}

	/* set the number of PortMapping entries */
#ifdef TR181	
	snprintf(buf, sizeof(buf), "%s.PortMappingNumberOfEntries", DEVICE_NAT);	
	sprintf(count_inst, "%s", __get_parameter_value(buf, tmp));

	snprintf(buf, sizeof(buf), "%s.PortMapping", DEVICE_NAT);
	lib_resolve_node(buf, &node);
	count = lib_get_children(node, &children);
	if(children) {
		lib_destroy_children(children);
		children = NULL;
	}

	if(count != atoi(count_inst)) {
		sprintf(count_inst, "%d", count);
		snprintf(buf, sizeof(buf), "%s.PortMappingNumberOfEntries", DEVICE_NAT);
		__set_parameter_value(buf, count_inst);

		lib_resolve_node(buf, &node);

		if(node->noc == 1 || node->noc == 2) {
			//sprintf(buf, "%s", pm_num_buf);
			//add_inform_parameter(buf, 1);
			add_inform_parameter(buf, 1);
		}
	}
#endif

#ifdef TR098
	tr_log(LOG_DEBUG, "update_port_mapping - reassign the count (start)");
	for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit ++) {
		int wd_inst = 0;

#ifdef ASUSWRT
		wd_inst = get_wd_inst_by_wan_unit(unit);
#else 	/* DSL_ASUSWRT */
		if (unit == 0)
			wd_inst = pri_wan_inst;
		else if (unit == 1)
			wd_inst = sec_wan_inst;
#endif

#ifdef RTCONFIG_XDSL
#ifdef ASUSWRT		
		if (wd_inst == IGD_WANDEVICE_WAN_INST)
#else 	/* DSL_ASUSWRT */
		if (wd_inst == IGD_WANDEVICE_DSL_INST)			
#endif
		{
			xdsl_reset_portmapping_count(unit, wd_inst);
		}
		else
#endif				
		if (wd_inst) {
#ifdef ASUSWRT
			snprintf(prefix, sizeof(prefix), "wan%d_", unit);
			wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
#else 	/* DSL_ASUSWRT */
			snprintf(prefix, sizeof(prefix), "wan%d_", convert_wd_inst_to_wan_unit(wd_inst));
			tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);
#endif
			snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnection", IGD_WANDEVICE, wd_inst);
			reset_portmapping_count(buf);

			if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp")) {
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection", IGD_WANDEVICE, wd_inst);
				reset_portmapping_count(buf);
			}	
		}
	}
	tr_log(LOG_DEBUG, "%s - reassign the count (end)", __FUNCTION__);
#endif
	
	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

static int set_eth_enable(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);
	int res = 0;
	char *value_conv = NULL;

	value_conv = (!war_strcasecmp(value, "1") || !war_strcasecmp(value, "true")) ? "1" : "0";

	memset(prefix, 0, sizeof(prefix));
#ifdef TR098
	snprintf(prefix, sizeof(prefix), "%s", eth_wanip_prefix_by_path(path, tmp));
#endif
#ifdef TR181
	snprintf(prefix, sizeof(prefix), "%s", ethernet_prefix_by_path(path, tmp));
	if(strncmp(prefix, "lan_", 4) == 0)
		return -1;
#endif

	if (strlen(prefix) == 0)
		return -1;
#ifdef TR098
#if defined(ASUSWRT) && defined(RTCONFIG_XDSL)
	if (check_dsl_wan_device_by_path(path))
		res = set_nvram("dslx_link_enable", value_conv);
	else
#endif
#endif

#ifdef ASUSWRT		
	res = set_nvram(strcat_r(prefix, "enable", tmp), value_conv);
#else 	/* DSL_ASUSWRT */
	res = set_nvram(convert_prefix_to_pvc(prefix, tmp), "Active", !strcmp(value_conv, "1") ? "Yes" : "No");
#endif

	return res;
}

static int get_eth_enable(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0}, tmp[32] = {0}, buf[16] = {0};
	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));
#ifdef TR098
	snprintf(prefix, sizeof(prefix), "%s", eth_wanip_prefix_by_path(path, tmp));
#endif
#ifdef TR181
	snprintf(prefix, sizeof(prefix), "%s", ethernet_prefix_by_path(path, tmp));
	if(strncmp(prefix, "lan_", 4) == 0) {
		*value = strdup("true");
		return 0;
	}
#endif

	if (strlen(prefix) == 0) {
		*value = strdup("");
		return 0;
	}

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%s", nvram_match(strcat_r(prefix, "enable", tmp), "1") ? "true" : "false");
#else 	/* DSL_ASUSWRT */
	snprintf(buf, sizeof(buf), "%s", tcapi_match(convert_prefix_to_pvc(prefix, tmp), "Active", "Yes") ? "true" : "false");
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_eth_connectionstatus(node_t node, char *arg, char **value)
{
	char *status = NULL;
	int unit = 0;
	char *path = lib_node2path(node);
#ifdef TR098
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0};

	unit = get_wan_unit_by_path(path);

	/* check prefix */
	memset(prefix, 0, sizeof(prefix));
	eth_wanip_prefix_by_path(path, prefix);
	if (strlen(prefix) == 0)
		unit = -1;
#endif
#ifdef TR181
	unit = ethernet_unit_by_path(path);

	if (unit == -1)	/* something wrong */
		return -1;
#endif
	switch (get_wan_state(unit)) {
		case WAN_STATE_CONNECTING:
			status = "Connecting";
			break;
		case WAN_STATE_CONNECTED:
			status = "Connected";
			break;
		case WAN_STATE_DISCONNECTED:
			status = "Disconnected";
			break;
		default:
			status = "Unconfigured";
	}

	*value = strdup(status);
	return *value ? 0 : -1;
}

static int get_eth_dnsservers(node_t node, char *arg, char **value)
{
	char buf[64] = {0}, prefix[16] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *wan_proto = NULL, *wan_dnsenable = NULL, *wan_dns = NULL;
#else 	/* DSL_ASUSWRT */
	char wan_dns[64] = {0};
#endif
	char *dns = NULL, *first = NULL, *next = NULL;
	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));
#ifdef TR098
	sprintf(prefix, "%s", eth_wanip_prefix_by_path(path, tmp));
#endif
#ifdef TR181
	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));
#endif

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	wan_dnsenable = nvram_safe_get(strcat_r(prefix, "dnsenable_x", tmp));

	memset(buf, 0, sizeof(buf));

#ifdef TR098
	if(strstr(path, "WANIPConnection") != NULL) {	/* WANIPConnection */
		if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp") 
#ifdef RTCONFIG_XDSL
			|| !strcmp(wan_proto, "pppoa")
#endif		
			) {
			if(!strcmp(wan_dnsenable, "1")) {
				wan_dns = nvram_safe_get(strcat_r(prefix, "xdns", tmp));
				first = next = strdup(wan_dns);
				while((dns = strsep(&next, " "))) {
					if(*dns)
						sprintf(buf + strlen(buf), "%s%s", strlen(buf) ? "," : "", dns);
				}
				free(first);
			}
			else
			{
				wan_dns = nvram_safe_get(strcat_r(prefix, "dns1_x", tmp));
				if (*wan_dns && strcmp(wan_dns, "0.0.0.0"))
					sprintf(buf, "%s", wan_dns);

				wan_dns = nvram_safe_get(strcat_r(prefix, "dns2_x", tmp));
				if (*wan_dns && strcmp(wan_dns, "0.0.0.0"))
					sprintf(buf + strlen(buf), "%s%s", strlen(buf) ? "," : "", wan_dns);
			}
		}
		else
		{
			if(!strcmp(wan_proto, "dhcp")
#ifdef RTCONFIG_XDSL
				|| (!strcmp(wan_proto, "mer") && !strcmp(wan_dnsenable, "1"))
#endif	
				) {
				wan_dns = nvram_safe_get(strcat_r(prefix, "dns", tmp));
				first = next = strdup(wan_dns);
				while((dns = strsep(&next, " "))) {
					if(*dns)
						sprintf(buf + strlen(buf), "%s%s", strlen(buf) ? "," : "", dns);
				}
				free(first);
			}
			else
			{
				wan_dns = nvram_safe_get(strcat_r(prefix, "dns1_x", tmp));
				if (*wan_dns && strcmp(wan_dns, "0.0.0.0"))
					sprintf(buf, "%s", wan_dns);

				wan_dns = nvram_safe_get(strcat_r(prefix, "dns2_x", tmp));
				if (*wan_dns && strcmp(wan_dns, "0.0.0.0"))
					sprintf(buf + strlen(buf), "%s%s", strlen(buf) ? "," : "", wan_dns);
			}
		}
	}
	else {	/* WANPPPConnection */
		wan_dns = nvram_safe_get(strcat_r(prefix, "dns", tmp));
		first = next = strdup(wan_dns);
		while((dns = strsep(&next, " "))) {
			if(*dns)
				sprintf(buf + strlen(buf), "%s%s", strlen(buf) ? "," : "", dns);
		}
		free(first);
	}
#endif

#ifdef TR181
	wan_dns = nvram_safe_get(strcat_r(prefix, "dns", tmp));
	first = next = strdup(wan_dns);
	while((dns = strsep(&next, " "))) {
		if(*dns)
			sprintf(buf + strlen(buf), "%s%s", strlen(buf) ? "," : "", dns);
	}
	free(first);
#endif

#else 	/* DSL_ASUSWRT */
#ifdef TR098
	if(strstr(path, "WANIPConnection") != NULL) {	/* WANIPConnection */
		char dns1[32] = {0}, dns2[32] = {0}, pvc[32] = {0};

		snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
		if (tcapi_match(pvc, "ISP", "0")) {	/* dhcp */
			if (tcapi_match(pvc, "DNS_type", "1")) {
				tcapi_get(pvc, "Primary_DNS", dns1);
				if (strlen(dns1) && strcmp(dns1, "0.0.0.0"))
					sprintf(buf, "%s", dns1);

				tcapi_get(pvc, "Secondary_DNS", dns2);
				if (strlen(dns2) && strcmp(dns2, "0.0.0.0"))
					sprintf(buf + strlen(buf), "%s%s", strlen(buf) ? "," : "", dns2);
			}
			else
			{
				tcapi_get(WANDUCK_NODE, strcat_r(prefix, "dns", tmp), wan_dns);
				first = next = strdup(wan_dns);
				while((dns = strsep(&next, " "))) {
					if(*dns)
						sprintf(buf + strlen(buf), "%s%s", strlen(buf) ? "," : "", dns);
				}
				free(first);
			}
		}
		else if (tcapi_match(pvc, "ISP", "1")){ 	/* static */
			tcapi_get(pvc, "Primary_DNS", dns1);
			if (strlen(dns1) && strcmp(dns1, "0.0.0.0"))
				sprintf(buf, "%s", dns1);

			tcapi_get(pvc, "Secondary_DNS", dns2);
			if (strlen(dns2) && strcmp(dns2, "0.0.0.0"))
				sprintf(buf + strlen(buf), "%s%s", strlen(buf) ? "," : "", dns2);			
		}
		else
			return -1;
	}
	else {	/* WANPPPConnection */
		tcapi_get(WANDUCK_NODE, strcat_r(prefix, "dns", tmp), wan_dns);
		first = next = strdup(wan_dns);
		while((dns = strsep(&next, " "))) {
			if(*dns)
				sprintf(buf + strlen(buf), "%s%s", strlen(buf) ? "," : "", dns);
		}
		free(first);	
	}
#endif

#ifdef TR181
	tcapi_get(WANDUCK_NODE, strcat_r(prefix, "dns", tmp), wan_dns);
	first = next = strdup(wan_dns);
	while((dns = strsep(&next, " "))) {
		if(*dns)
			sprintf(buf + strlen(buf), "%s%s", strlen(buf) ? "," : "", dns);
	}
	free(first);
#endif
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_eth_addressingtype(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[16] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *wan_proto = NULL;
	char *wan_dhcpenable = NULL;
#endif
	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));
#ifdef TR098
	sprintf(prefix, "%s", eth_wanip_prefix_by_path(path, tmp));
#endif
#ifdef TR181
	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));
#endif

	//if (strlen(prefix) == 0 || strncmp(prefix, "wan", 3))
	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	wan_dhcpenable = nvram_safe_get(strcat_r(prefix, "dhcpenable_x", tmp));

	if (!strcmp(prefix, "lan_")) {
		if (sw_mode == SW_MODE_ROUTER)
			snprintf(buf, sizeof(buf), "Static");
		else if (sw_mode == SW_MODE_AP)
			snprintf(buf, sizeof(buf), "%s", !strcmp(wan_proto, "dhcp") ? "DHCP" : "Static");
	}
	else if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp") 
#ifdef RTCONFIG_XDSL
		|| !strcmp(wan_proto, "pppoa")
#endif
		) {
		snprintf(buf, sizeof(buf), "%s", !strcmp(wan_dhcpenable, "1") 
#ifdef RTCONFIG_XDSL
										|| !strcmp(wan_dhcpenable, "2") 
#endif
										? "DHCP" : "Static");
	}
	else
		snprintf(buf, sizeof(buf), "%s", !strcmp(wan_proto, "dhcp")
#ifdef RTCONFIG_XDSL
										|| (!strcmp(wan_proto, "mer") && (!strcmp(wan_dhcpenable, "1")))
#endif
										 ? "DHCP" : "Static");

#else 	/* DSL_ASUSWRT */
	if (!strcmp(prefix, "lan_"))
		snprintf(buf, sizeof(buf), "Static");
	else
		snprintf(buf, sizeof(buf), "%s", tcapi_match(convert_prefix_to_pvc(prefix, tmp), "ISP", "1")? "Static" : "DHCP");
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_availablelanguages(node_t node, char *arg, char ** value)
{
	FILE *fp;
	char *p = NULL;
	char buf[32] = {0}, tmp[10] = {0}, bufLAN[256] = {0};	

#ifdef ASUSWRT
	fp = fopen("/www/Lang_Hdr.txt", "r");
#else 	/* DSL_ASUSWRT */
	fp = fopen("/boaroot/cgi-bin/Lang_Hdr", "r");
#endif
	if (fp == NULL)
		return -1;

	while (fgets(buf, sizeof(buf), fp)!= NULL) {
		if((p = strstr(buf, "LANG_")) != NULL) {
			memset(tmp, 0, sizeof(tmp));
			strncpy(tmp, p + 5, 2);
			strcat(bufLAN, tmp);
			strcat(bufLAN, ",");
		}
	}
	fclose(fp);
	bufLAN[strlen(bufLAN) - 1] = '\0';


	*value = strdup(bufLAN);
	return *value ? 0 : -1;
}

int deny_add_object()
{
	if(update_flag == 1)
		return -1;
	else
		return 0;
}

int deny_del_object()
{
	if(update_flag == 1)
		return -1;
	else
		return 0;
}

int add_manageable_device(char *oui, char *serial, char *class)
{
	char buf[128] = {0}, tmp[64] = {0};
	node_t *children = NULL;
	node_t node;
	int count = 0, rec_count = 0, need_add = 1, i = 0;

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);
	tr_log(LOG_DEBUG, "%s - oui: %s, serial: %s, class: %s", __FUNCTION__, oui, serial, class);

	if(lib_resolve_node(MNG_DEVICE, &node) == 0) {
		rec_count = count = lib_get_children(node, &children);
		
		while(count > 0) {
			char name[16] = {0};
			count--;

			if(lib_get_property(children[count], "name", name) == 0) {
				if(string_is_digits(name) == 1) {
					char oui_inst[8] = {0}, serial_inst[64] = {0}, class_inst[64] = {0};
					char oui_buf[64] = {0}, serial_buf[64] = {0}, class_buf[64] = {0};

					/* get the data of ManufacturerOUI */
					sprintf(oui_buf, "%s.%s.ManufacturerOUI", MNG_DEVICE, name);
					sprintf(oui_inst, "%s", __get_parameter_value(oui_buf, tmp));

					/* get the data of SerialNumber */
					sprintf(serial_buf, "%s.%s.SerialNumber", MNG_DEVICE, name);
					sprintf(serial_inst, "%s", __get_parameter_value(serial_buf, tmp));

					/* get the data of ProductClass */
					sprintf(class_buf, "%s.%s.ProductClass", MNG_DEVICE, name);
					sprintf(class_inst, "%s", __get_parameter_value(class_buf, tmp));

					if(!strcmp(oui, oui_inst) && !strcmp(serial, serial_inst) && !strcmp(class, class_inst)) {
						need_add = 0;
						break;
					}
				}
			}
		}

		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}
	}

	if(need_add) {
		char count_str[8] = {0};

		memset(buf, 0, sizeof(buf));
		sprintf(buf, "%s.", MNG_DEVICE);
		if((i = add_object(buf, strlen(buf))) < 9000) {
			/* set ManufacturerOUI, SerialNumber, ProductClass */
			sprintf(buf, "%s.%d.ManufacturerOUI", MNG_DEVICE, i);
			__set_parameter_value(buf, oui);

			sprintf(buf, "%s.%d.SerialNumber", MNG_DEVICE, i);		
			__set_parameter_value(buf, serial);

			sprintf(buf, "%s.%d.ProductClass", MNG_DEVICE, i);		
			__set_parameter_value(buf, class);

			/* set the number of ManageableDeviceNumberOfEntries */
			snprintf(buf, sizeof(buf), "%s", MNG_DEVICE_NUM);
			sprintf(count_str, "%d", rec_count + 1);
			__set_parameter_value(buf, count_str);

			add_inform_parameter(MNG_DEVICE_NUM, 1);
		}
		else
			return -1;
	}

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

int del_manageable_device(char *oui, char *serial, char *class)
{
	node_t *children = NULL;
	node_t node;
	int count = 0, rec_count = 0, need_del = 0;
	char buf[128] = {0}, tmp[64] = {0};

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);
	tr_log(LOG_DEBUG, "%s - oui: %s, serial: %s, class: %s", __FUNCTION__, oui, serial, class);

	if(lib_resolve_node(MNG_DEVICE, &node) == 0) {
		rec_count = count = lib_get_children(node, &children);
		
		while(count > 0) {
			char name[16] = {0};
			count--;

			if(lib_get_property(children[count], "name", name) == 0) {
				if(string_is_digits(name) == 1) {
					char oui_inst[8] = {0}, serial_inst[64] = {0}, class_inst[64] = {0};
					char oui_buf[64] = {0}, serial_buf[64] = {0}, class_buf[64] = {0};
 
					/* get the data of ManufacturerOUI */
					sprintf(oui_buf, "%s.%s.ManufacturerOUI", MNG_DEVICE, name);
					sprintf(oui_inst, "%s", __get_parameter_value(oui_buf, tmp));

					/* get the data of SerialNumber */
					sprintf(serial_buf, "%s.%s.SerialNumber", MNG_DEVICE, name);
					sprintf(serial_inst, "%s", __get_parameter_value(serial_buf, tmp));

					/* get the data of ProductClass */
					sprintf(class_buf, "%s.%s.ProductClass", MNG_DEVICE, name);
					sprintf(class_inst, "%s", __get_parameter_value(class_buf, tmp));

					if(!strcmp(oui, oui_inst) && !strcmp(serial, serial_inst) && !strcmp(class, class_inst)) {
						need_del = atoi(name);
						break;
					}
				}
			}
		}

		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}
	}

	if(need_del) {
		char count_str[8] = {0};

		sprintf(buf, "%s.%d.", MNG_DEVICE, need_del);
		tr_log(LOG_DEBUG, "The path for delete: %s", buf);
		delete_object(buf, strlen(buf));

		/* set the number of ManageableDeviceNumberOfEntries */
		snprintf(buf, sizeof(buf), "%s", MNG_DEVICE_NUM);
		sprintf(count_str, "%d", rec_count - 1);
		__set_parameter_value(buf, count_str);

		add_inform_parameter(MNG_DEVICE_NUM, 1);
	}

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

int update_manageable_device()
{
	char buf[128] = {0};
	FILE *fp;
#ifdef ASUSWRT
	char *mng_device_file = "/tmp/dhcpc_lease_list";
#else 	/* DSL_ASUSWRT */
	char *mng_device_file = "/etc/devices.conf";
#endif

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	if (!check_if_file_exist(mng_device_file))
		return 0;

	if ((fp = fopen(mng_device_file, "r")) == NULL) {
		tr_log(LOG_DEBUG, "can't open file(%s)", mng_device_file);
		return -1;
	}

	while(fgets(buf, sizeof(buf), fp) != NULL) {
		char *oui = NULL, *serial = NULL, *class = NULL, *hwaddr = NULL;
		if ((vstrsep(buf, ",", &oui, &serial, &class, &hwaddr) != 4))
			continue;
		tr_log(LOG_DEBUG, "update oui: %s, serial: %s, class: %s, hwaddr: %s", oui, serial, class, hwaddr);
		add_manageable_device(oui, serial, class);
	}
	fclose(fp);

	//unlink(mng_device_file);

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

#ifdef RTCONFIG_XDSL	/* start of RTCONFIG_XDSL */
char *get_xdsl_prefix(char *path, char *prefix)
{
	char buf[128] = {0}, tmp[32] = {0};

	snprintf(buf, sizeof(buf), "%s.X_ASUS_DSLIndex", path);
	sprintf(prefix, "%s", __get_parameter_value(buf, tmp));

	return prefix;
}

char *find_xdsl_unsed_index(char *path, char *prefix)
{
	int i = 0;
	char dsl_index[16] = {0}, buf[256] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
#endif 

#if defined(RTCONFIG_DSL) || defined(TCSUPPORT_WAN_ATM)
#ifdef RTCONFIG_DSL
	if (nvram_match("dslx_transmode", ADSL_TRANS_MODE)) {
#endif
#ifdef TCSUPPORT_WAN_ATM
#ifdef TCSUPPORT_WAN_PTM
	if (tcapi_match("Wan_Common", "DSLMode", ADSL_TRANS_MODE)) {
#else
	{
#endif
#endif
		for (i = 0; i < DSL_NUM; i++) {
#ifdef RTCONFIG_DSL
			snprintf(dsl_index, sizeof(dsl_index), "dsl%d", i);
			if (!nvram_match(strcat_r(dsl_index, "_enable", tmp), "1")) {
				sprintf(prefix, "dsl%d", i);
#endif
#ifdef TCSUPPORT_WAN_ATM
			snprintf(dsl_index, sizeof(dsl_index), "%s%d", WAN_XTM_PREFIX, i);
			if (!tcapi_match(dsl_index, "Active", "Yes")) {	
				sprintf(prefix, "%s%d", WAN_XTM_PREFIX, i);
#endif
				snprintf(buf, sizeof(buf), "%s.X_ASUS_DSLIndex", path);
				__set_parameter_value(buf, prefix);
				break;
			}
		}		
	}
#endif

#if defined(RTCONFIG_VDSL) || TCSUPPORT_WAN_PTM
#ifdef RTCONFIG_VDSL	
	if (nvram_match("dslx_transmode", VDSL_TRANS_MODE)) {
#endif
#ifdef TCSUPPORT_WAN_PTM
#ifdef TCSUPPORT_WAN_ATM
		if (tcapi_match("Wan_Common", "DSLMode", VDSL_TRANS_MODE)) {
#else
		{
#endif
#endif
		for (i = 0; i < DSL_NUM; i++) {
#ifdef RTCONFIG_VDSL
			if (i == 0)
				snprintf(dsl_index, sizeof(dsl_index), "dsl8");
			else
				snprintf(dsl_index, sizeof(dsl_index), "dsl8.%d", i);

			if (!nvram_match(strcat_r(dsl_index, "_enable", tmp), "1")) {
				if (i == 0)
					sprintf(prefix, "dsl8");
				else
					sprintf(prefix, "dsl8.%d", i);
#endif
#ifdef TCSUPPORT_WAN_PTM
			if (i == 0)
				snprintf(dsl_index, sizeof(dsl_index), "%s8", WAN_XTM_PREFIX);
			else
				snprintf(dsl_index, sizeof(dsl_index), "%s%d", WAN_PTM_EXT_PREFIX, i - 1);

			if (!tcapi_match(dsl_index, "Active", "Yes")) {	
				if (i == 0)
					sprintf(prefix, "%s8", WAN_XTM_PREFIX);
				else
					sprintf(prefix, "%s%d", WAN_PTM_EXT_PREFIX, i - 1);
#endif

				snprintf(buf, sizeof(buf), "%s.X_ASUS_DSLIndex", path);
				__set_parameter_value(buf, prefix);
				break;
			}
		}		
	}	
#endif

	return prefix;
}

static int get_dsl_if_config(node_t node, char *arg, char **value)
{
	char buf[32] = {0};

	memset(buf, 0, sizeof(buf));

#ifdef ASUSWRT
	if (check_if_file_exist(DSL_INFO_FILE)) {
		FILE *fp = NULL;
		char line_buf[64] = {0};
		
		fp = fopen(DSL_INFO_FILE, "r");
		if (fp == NULL) {
			*value = strdup("");
			return 0;
		}

		while (fgets(line_buf, sizeof(line_buf), fp)!= NULL) {
			char *equal_ptr = strchr(line_buf, '=');
			char *value_ptr = NULL;

			if (equal_ptr) {
				*equal_ptr = '\0';
				if (!strcmp(line_buf, arg)) {
					equal_ptr++;
					value_ptr = equal_ptr;
					//tr_log(LOG_DEBUG, "%s - name=%s, value=%s", __FUNCTION__, line_buf, value_ptr);
					snprintf(buf, sizeof(buf), "%s", value_ptr);
					break;
				}
			}
		}

		fclose(fp);
	}
#else 	/* DSL_ASUSWRT */

	tcapi_get(INFO_ADSL_NODE, arg, buf);
#endif

	*value = strdup(buf);

	return 0;
}

static int set_dsl_enable(node_t node, char *arg, char *value)
{
	char prefix[sizeof("dslXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node->parent);
	char *value_conv = NULL;

	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';
	value_conv = (!war_strcasecmp(value, "1") || !war_strcasecmp(value, "true")) ? "1" : "0";
	get_xdsl_prefix(path, prefix);

	/* find unsed index (prefix such as dslX or dsl8.X for ASUSWRT, Wan_PVCX for DSL_ASUSWRT) if no prefix */
	if (strlen(prefix) == 0)
		find_xdsl_unsed_index(path, prefix);

	if (strlen(prefix) == 0
#ifdef ASUSWRT
	 || strncmp(prefix, "dsl", 3)
#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) && defined(TCSUPPORT_WAN_PTM)
	 || (strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
	 	&& strncmp(prefix, WAN_PTM_EXT_PREFIX, strlen(WAN_PTM_EXT_PREFIX))
	 	)
#else
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	 || strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
#endif
#endif
#endif 
	 	)
		return -1;

#ifdef ASUSWRT
	return set_nvram(strcat_r(prefix, "_enable", tmp), value_conv);
#else 	/* DSL_ASUSWRT */
	return set_nvram(convert_prefix_to_pvc(prefix, tmp), "Active", !strcmp(value_conv, "1") ? "Yes" : "No");
#endif
}

static int get_dsl_enable(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0}, buf[16] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
#endif 
	char *path = lib_node2path(node->parent);

	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';
	get_xdsl_prefix(path, prefix);

	if (strlen(prefix) == 0
#ifdef ASUSWRT
	 || strncmp(prefix, "dsl", 3)
#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) && defined(TCSUPPORT_WAN_PTM)
	 || (strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
	 	&& strncmp(prefix, WAN_PTM_EXT_PREFIX, strlen(WAN_PTM_EXT_PREFIX))
	 	)
#else
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	 || strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
#endif
#endif
#endif 
	) {
		*value = strdup("");
		return 0;
	}

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%s", nvram_match(strcat_r(prefix, "enable", tmp), "1") ? "true" : "false");
#else 	/* DSL_ASUSWRT */
	snprintf(buf, sizeof(buf), "%s", tcapi_match(prefix, "Active", "Yes") ? "true" : "false");
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_dsl_link_type(node_t node, char *arg, char *value)
{
	char prefix[sizeof("dslXXXXXXXXXX_")] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
#endif
	char *path = lib_node2path(node->parent);
	int res = 0;

	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';
	get_xdsl_prefix(path, prefix);

	if (!strcasecmp(value, "PPPoE"))	/* The value PPPoE has always been DEPRECATED */
		return -1;

	/* find unsed index (prefix such as dslX or dsl8.X for ASUSWRT, Wan_PVCX for DSL_ASUSWRT) if no prefix */
	if (strlen(prefix) == 0)
		find_xdsl_unsed_index(path, prefix);

	if (strlen(prefix) == 0
#ifdef ASUSWRT
	 || strncmp(prefix, "dsl", 3)
#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) && defined(TCSUPPORT_WAN_PTM)
	 || (strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
	 	&& strncmp(prefix, WAN_PTM_EXT_PREFIX, strlen(WAN_PTM_EXT_PREFIX))
	 	)
#else
#if defined(CSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	 || strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
#endif
#endif
#endif 
	 	)
		return -1;

#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
	if (nvram_match("dslx_transmode", ADSL_TRANS_MODE)) {	/* for ADSL */
		if (!strcmp(value, "EoA"))
			res = set_nvram(strcat_r(prefix, "_proto", tmp), "pppoe");
		else if (!strcmp(value, "PPPoA"))
			res = set_nvram(strcat_r(prefix, "_proto", tmp), "pppoa");
		else if (!strcmp(value, "IPoA"))
			res = set_nvram(strcat_r(prefix, "_proto", tmp), "ipoa");
		else if (!strcmp(value, "MER"))
			res = set_nvram(strcat_r(prefix, "_proto", tmp), "mer");
	}
#endif

#ifdef RTCONFIG_VDSL
	if (nvram_match("dslx_transmode", VDSL_TRANS_MODE))	/* for VDSL */
		return -1;	/* can't set for VDSL */
#endif

#else 	/* DSL_ASUSWRT */
#ifdef TCSUPPORT_WAN_ATM
	if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", ADSL_TRANS_MODE)) || !support_adsl_vdsl) {	/* for ADSL */
		if (!strcmp(value, "EoA")) {
			if (!strcmp(prefix, "Wan_PVC0")) {
				set_nvram(prefix, "ENCAP", PPPOE_LLC);
				res = set_nvram(prefix, "ISP", "2");	/* pppoe/pppoa */
			}
			else
			{
				set_nvram(prefix, "ENCAP", BRIDGED_ONLY_LLC);
				res = set_nvram(prefix, "ISP", "3");	/* bridge */
			}
		}
		else if (!strcmp(value, "PPPoA")) {
			if (!strcmp(prefix, "Wan_PVC0")) {
				set_nvram(prefix, "ENCAP", PPPOA_LLC);
				res = set_nvram(prefix, "ISP", "2");	/* pppoe/pppoa */
			}
			else
			{
				set_nvram(prefix, "ENCAP", BRIDGED_ONLY_LLC);
				res = set_nvram(prefix, "ISP", "3");	/* bridge */
			}
		}
		else if (!strcmp(value, "IPoA")) {
			if (!strcmp(prefix, "Wan_PVC0")) {
				set_nvram(prefix, "ENCAP", ROUTED_IP_LLC);
				res = set_nvram(prefix, "ISP", "0");	/* dhcp */
			}
			else
			{
				set_nvram(prefix, "ENCAP", BRIDGED_ONLY_LLC);
				res = set_nvram(prefix, "ISP", "3");	/* bridge */
			}
		}
	}
#endif

#ifdef TCSUPPORT_WAN_PTM
	if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", VDSL_TRANS_MODE)) || !support_adsl_vdsl) 	/* for VDSL */		
		return -1;	/* can't set for VDSL */
#endif
#endif	

	return res;
}

static int get_dsl_link_type(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0}, buf[16] = {0};
	char *path = lib_node2path(node->parent);
#ifdef ASUSWRT
	char tmp[32] = {0};
	char *dsl_proto = NULL;
#else 	/* DSL_ASUSWRT */
	char dsl_proto[8] = {0};	
#endif

	memset(buf, 0, sizeof(buf));
	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';
	get_xdsl_prefix(path, prefix);

	if (strlen(prefix) == 0
#ifdef ASUSWRT
	 || strncmp(prefix, "dsl", 3)
#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) && defined(TCSUPPORT_WAN_PTM)
	 || (strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
	 	&& strncmp(prefix, WAN_PTM_EXT_PREFIX, strlen(WAN_PTM_EXT_PREFIX))
	 	)
#else
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	 || strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
#endif
#endif
#endif 
	) {
		*value = strdup("");
		return 0;
	}

#ifdef ASUSWRT
	dsl_proto = nvram_safe_get(strcat_r(prefix, "_proto", tmp));

#ifdef RTCONFIG_DSL
	if (nvram_match("dslx_transmode", ADSL_TRANS_MODE)) {	/* for ADSL */
		if (!strcasecmp(dsl_proto, "pppoe"))
			snprintf(buf, sizeof(buf), "EoA");
		else if (!strcasecmp(dsl_proto, "pppoa"))
			snprintf(buf, sizeof(buf), "PPPoA");
		else if (!strcasecmp(dsl_proto, "ipoa"))
			snprintf(buf, sizeof(buf), "IPoA");
		else if (!strcasecmp(dsl_proto, "mer"))
			snprintf(buf, sizeof(buf), "MER");
		else
			snprintf(buf, sizeof(buf), "Unconfigured");
	}
#endif
#ifdef RTCONFIG_VDSL
	if (nvram_match("dslx_transmode", VDSL_TRANS_MODE))	/* for VDSL */
		snprintf(buf, sizeof(buf), "Unconfigured");
#endif

#else 	/* DSL_ASUSWRT */
	tcapi_get(prefix, "ISP", dsl_proto);

#ifdef TCSUPPORT_WAN_ATM
	if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", ADSL_TRANS_MODE)) || !support_adsl_vdsl) {	/* for ADSL */		
		char dsl_encap[32] = {0};
		tcapi_get(prefix, "ENCAP", dsl_encap);

		if (!strcmp(dsl_proto, "2")) {	/* pppoe/pppoa */
			if (strstr(dsl_encap, "PPPoE"))
				snprintf(buf, sizeof(buf), "EoA");
			else if (strstr(dsl_encap, "PPPoA"))
				snprintf(buf, sizeof(buf), "PPPoA");
			else
				snprintf(buf, sizeof(buf), "EoA");	
		}
		else if (!strcmp(dsl_proto, "0") || !strcmp(dsl_proto, "1")) {	/* dhcp/static */
			if (strstr(dsl_encap, "Routed IP"))
				snprintf(buf, sizeof(buf), "IPoA");
			else if (strstr(dsl_encap, "Bridged IP"))
				snprintf(buf, sizeof(buf), "EoA");
			else
				snprintf(buf, sizeof(buf), "EoA");
		}
		else if (!strcmp(dsl_proto, "0"))	/* bridge */
			snprintf(buf, sizeof(buf), "EoA");
		else
			snprintf(buf, sizeof(buf), "Unconfigured");	
	}
#endif

#ifdef TCSUPPORT_WAN_PTM
	if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", VDSL_TRANS_MODE)) || !support_adsl_vdsl)	/* for VDSL */
		snprintf(buf, sizeof(buf), "Unconfigured");
#endif
#endif
	
	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_dsl_dest_addr(node_t node, char *arg, char *value)
{
	char prefix[sizeof("dslXXXXXXXXXX_")] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
#endif
	char *path = lib_node2path(node->parent);
#ifdef TR098
	char *colon_ptr = NULL;
#endif
	char *slash_ptr = NULL;
	int res = 0;

#ifdef TR098
	if (!strstr(value, "PVC") || !strchr(value, ':') || !strchr(value, '/'))	/* need to include PVC str, ":", "/" */
#endif
#ifdef TR181
	if (!strchr(value, '/'))	/* need to include "/" */
#endif
		return -1;

	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';
	get_xdsl_prefix(path, prefix);

	/* find unsed index (prefix such as dslX or dsl8.X for ASUSWRT, Wan_PVCX for DSL_ASUSWRT) if no prefix */
	if (strlen(prefix) == 0)
		find_xdsl_unsed_index(path, prefix);	

	if (strlen(prefix) == 0
#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
	 || strncmp(prefix, "dsl", 3)
#endif

#ifdef RTCONFIG_VDSL
	 || !strncmp(prefix, "dsl8", 4)
#endif

#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	 || strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
#endif
#ifdef TCSUPPORT_WAN_PTM
	 || !strncmp(prefix, WAN_PTM_EXT_PREFIX, strlen(WAN_PTM_EXT_PREFIX))
#endif
#endif
	) /* dsl8 ~ dsl8.x for ASUSWRT don't support, WanExt_PVC8e0 ~ WanExt_PVC8e7 for DSL_ASUSWRT don't support */
		return -1;

#ifdef TR098
	colon_ptr = strchr(value, ':');
	colon_ptr++;
#endif
	slash_ptr = strchr(value, '/');
	*slash_ptr = '\0';
	slash_ptr++;

#ifdef ASUSWRT
	/* set vpi */
#ifdef TR098
	res = set_nvram(strcat_r(prefix, "_vpi", tmp), skip_blanks(colon_ptr));
#endif
#ifdef TR181
	res = set_nvram(strcat_r(prefix, "_vpi", tmp), skip_blanks(value));
#endif
	if (res < 0)
		return -1;

	/* set vpi */
	res = set_nvram(strcat_r(prefix, "_vci", tmp), slash_ptr);

#else 	/* DSL_ASUSWRT */
	/* set vpi */
#ifdef TR098
	res = set_nvram(prefix, "VPI", skip_blanks(colon_ptr));
#endif
#ifdef TR181
	res = set_nvram(prefix, "VPI", skip_blanks(value));
#endif
	if (res < 0)
		return -1;

	/* set vpi */
	res = set_nvram(prefix, "VCI", slash_ptr);

#endif

	return res;
}

static int get_dsl_dest_addr(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0}, buf[16] = {0};
	char *path = lib_node2path(node->parent);
#ifdef ASUSWRT
	char tmp[32] = {0};
#else 	/* DSL_ASUSWRT */
	char vpi[8] = {0}, vci[8] = {0};
#endif

	memset(buf, 0, sizeof(buf));
	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';
	get_xdsl_prefix(path, prefix);

	if (strlen(prefix) == 0
#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
	 || strncmp(prefix, "dsl", 3)
#endif

#ifdef RTCONFIG_VDSL
	 || !strncmp(prefix, "dsl8", 4)
#endif

#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	 || strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
#endif
#ifdef TCSUPPORT_WAN_PTM
	 || !strncmp(prefix, WAN_PTM_EXT_PREFIX, strlen(WAN_PTM_EXT_PREFIX))
#endif
#endif
	) { /* dsl8 ~ dsl8.x for ASUSWRT don't support, WanExt_PVC8e0 ~ WanExt_PVC8e7 for DSL_ASUSWRT don't support */
		*value = strdup("");
		return 0;
	}

	snprintf(buf, sizeof(buf), 
#ifdef TR098		
					"PVC: %s/%s",
#endif
#ifdef TR181
					 "%s/%s",
#endif
#ifdef ASUSWRT
		nvram_safe_get(strcat_r(prefix, "_vpi", tmp)), nvram_safe_get(strcat_r(prefix, "_vci", tmp)));
#else 	/* DSL_ASUSWRT */
		tcapi_get_string(prefix, "VPI", vpi), tcapi_get_string(prefix, "VCI", vci));
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_dsl_atm_encap(node_t node, char *arg, char *value)
{
	char prefix[sizeof("dslXXXXXXXXXX_")] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
#endif
	char *path = lib_node2path(node->parent);
	int res = -1;
#ifdef DSL_ASUSWRT
	char dsl_proto[8] = {0};
#endif
	
	if (strcmp(value, "LLC") && strcmp(value, "VCMUX"))
		return -1;

	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';
	get_xdsl_prefix(path, prefix);

	/* find unsed index (prefix such as dslX or dsl8.X for ASUSWRT, Wan_PVCX or WanExt_PVC8eX for DSL_ASUSWRT) if no prefix */
	if (strlen(prefix) == 0)
		find_xdsl_unsed_index(path, prefix);

	if (strlen(prefix) == 0
#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
	 || strncmp(prefix, "dsl", 3)
#endif

#ifdef RTCONFIG_VDSL
	 || !strncmp(prefix, "dsl8", 4)
#endif

#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	 || strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
#endif
#ifdef TCSUPPORT_WAN_PTM
	 || !strncmp(prefix, WAN_PTM_EXT_PREFIX, strlen(WAN_PTM_EXT_PREFIX))
#endif
#endif
	) /* dsl8 ~ dsl8.x for ASUSWRT don't support, WanExt_PVC8e0 ~ WanExt_PVC8e7 for DSL_ASUSWRT don't support */
		return -1;

#ifdef ASUSWRT
	tcapi_get(prefix, "ISP", wan_proto);

	if (!strcmp(value, "LLC"))
		res = set_nvram(strcat_r(prefix, "_encap", tmp), "0");
	else if (!strcmp(value, "VCMUX"))
		res = set_nvram(strcat_r(prefix, "_encap", tmp), "1");

#else 	/* DSL_ASUSWRT */
	char dsl_encap[32] = {0};

	tcapi_get(prefix, "ISP", dsl_proto);
	tcapi_get(prefix, "ENCAP", dsl_encap);

	if (!strcmp(dsl_proto, "2")) {	/* pppoe/pppoa */
		if (strstr(dsl_encap, "PPPoE")){	/* pppoe */
			if (!strcmp(value, "LLC"))
				res = set_nvram(prefix, "ENCAP", PPPOE_LLC);
			else if (!strcmp(value, "VCMUX"))
				res = set_nvram(prefix, "ENCAP", PPPOE_VCMUX);
		}
		else if (strstr(dsl_encap, "PPPoA")){	/* pppoa */
			if (!strcmp(value, "LLC"))
				res = set_nvram(prefix, "ENCAP", PPPOA_LLC);
			else if (!strcmp(value, "VCMUX"))
				res = set_nvram(prefix, "ENCAP", PPPOA_VCMUX);
		}
	}
	else if (!strcmp(dsl_proto, "0") || !strcmp(dsl_proto, "1")) {	/* dhcp/static */
		if (strstr(dsl_encap, "Routed")){
			if (!strcmp(value, "LLC"))
				res = set_nvram(prefix, "ENCAP", ROUTED_IP_LLC);
			else if (!strcmp(value, "VCMUX"))
				res = set_nvram(prefix, "ENCAP", ROUTED_IP_VCMUX);
		}
		else if (strstr(dsl_encap, "Bridged")){
			if (!strcmp(value, "LLC"))
				res = set_nvram(prefix, "ENCAP", BRIDGED_IP_LLC);
			else if (!strcmp(value, "VCMUX"))
				res = set_nvram(prefix, "ENCAP", BRIDGED_IP_VCMUX);
		}
	}
	else if (!strcmp(dsl_proto, "2")) {	/* bridge */
		if (!strcmp(value, "LLC"))
			res = set_nvram(prefix, "ENCAP", BRIDGED_ONLY_LLC);
		else if (!strcmp(value, "VCMUX"))
			res = set_nvram(prefix, "ENCAP", BRIDGED_ONLY_VCMUX);
	}
#endif

	return res;
}

static int get_dsl_atm_encap(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0}, buf[16] = {0};
	char *path = lib_node2path(node->parent);
#ifdef ASUSWRT
	char tmp[32] = {0};
	char *dsl_encap = NULL;
#else 	/* DSL_ASUSWRT */
	char dsl_encap[32] = {0};
#endif

	memset(buf, 0, sizeof(buf));
	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';
	get_xdsl_prefix(path, prefix);

	if (strlen(prefix) == 0
#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
	 || strncmp(prefix, "dsl", 3)
#endif

#ifdef RTCONFIG_VDSL
	 || !strncmp(prefix, "dsl8", 4)
#endif

#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	 || strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
#endif
#ifdef TCSUPPORT_WAN_PTM
	 || !strncmp(prefix, WAN_PTM_EXT_PREFIX, strlen(WAN_PTM_EXT_PREFIX))
#endif
#endif
	) { /* dsl8 ~ dsl8.x for ASUSWRT don't support, WanExt_PVC8e0 ~ WanExt_PVC8e7 for DSL_ASUSWRT don't support */
		*value = strdup("");
		return 0;
	}

#ifdef ASUSWRT
	dsl_encap = nvram_safe_get(strcat_r(prefix, "_encap", tmp));

	if (!strcmp(dsl_encap, "0"))
		snprintf(buf, sizeof(buf), "LLC");
	else if (!strcmp(dsl_encap, "1"))
		snprintf(buf, sizeof(buf), "VCMUX");

#else 	/* DSL_ASUSWRT */
	tcapi_get(prefix, "ENCAP", dsl_encap);

	if (strstr(dsl_encap, "LLC"))
		snprintf(buf, sizeof(buf), "LLC");
	else if (strstr(dsl_encap, "VC-Mux"))
		snprintf(buf, sizeof(buf), "VCMUX");	
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_dsl_atm_qos(node_t node, char *arg, char *value)
{
	char prefix[sizeof("dslXXXXXXXXXX_")] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
#endif
#ifdef TR098
	char *path = lib_node2path(node->parent);
#endif
#ifdef TR181
	char *path = lib_node2path(node->parent->parent);
#endif	
	int res = -1;

	if (strcmp(value, "UBR") && strcmp(value, "CBR") && strcmp(value, "VBR-rt") && strcmp(value, "VBR-nrt")
#ifdef ASUSWRT
		&& strcmp(value, "UBR-npcr") &&  strcmp(value, "GFR")
#endif
		)
		return -1;

	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';
	get_xdsl_prefix(path, prefix);

	/* find unsed index (prefix such as dslX or dsl8.X) if no prefix */
	if (strlen(prefix) == 0)
		find_xdsl_unsed_index(path, prefix);

	if (strlen(prefix) == 0
#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
	 || strncmp(prefix, "dsl", 3)
#endif

#ifdef RTCONFIG_VDSL
	 || !strncmp(prefix, "dsl8", 4)
#endif

#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	 || strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
#endif
#ifdef TCSUPPORT_WAN_PTM
	 || !strncmp(prefix, WAN_PTM_EXT_PREFIX, strlen(WAN_PTM_EXT_PREFIX))
#endif
#endif
	) /* dsl8 ~ dsl8.x for ASUSWRT don't support, WanExt_PVC8e0 ~ WanExt_PVC8e7 for DSL_ASUSWRT don't support */
		return -1;

#ifdef ASUSWRT
	if (!strcmp(value, "UBR-npcr"))
		res = set_nvram(strcat_r(prefix, "_svc_cat", tmp), "0");
	else if (!strcmp(value, "UBR"))
		res = set_nvram(strcat_r(prefix, "_svc_cat", tmp), "1");
	else if (!strcmp(value, "CBR"))
		res = set_nvram(strcat_r(prefix, "_svc_cat", tmp), "2");
	else if (!strcmp(value, "VBR-rt"))
		res = set_nvram(strcat_r(prefix, "_svc_cat", tmp), "3");
	else if (!strcmp(value, "GFR"))
		res = set_nvram(strcat_r(prefix, "_svc_cat", tmp), "4");
	else if (!strcmp(value, "VBR-nrt"))
		res = set_nvram(strcat_r(prefix, "_svc_cat", tmp), "5");					

#else 	/* DSL_ASUSWRT */
	if (!strcmp(value, "UBR"))
		res = set_nvram(prefix, "QOS", "ubr");
	else if (!strcmp(value, "CBR"))
		res = set_nvram(prefix, "QOS", "cbr");
	else if (!strcmp(value, "VBR-rt"))
		res = set_nvram(prefix, "QOS", "rt-vbr");
	else if (!strcmp(value, "VBR-nrt"))
		res = set_nvram(prefix, "QOS", "nrt-vbr");	
#endif

	return res;
}

static int get_dsl_atm_qos(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0}, buf[16] = {0};
#ifdef TR098
	char *path = lib_node2path(node->parent);
#endif
#ifdef TR181
	char *path = lib_node2path(node->parent->parent);
#endif
#ifdef ASUSWRT
	char tmp[32] = {0};
	char *dsl_svc_cat = NULL;
#else 	/* DSL_ASUSWRT */
	char dsl_svc_cat[16] = {0};
#endif

	memset(buf, 0, sizeof(buf));
	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';
	get_xdsl_prefix(path, prefix);

	if (strlen(prefix) == 0
#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
	 || strncmp(prefix, "dsl", 3)
#endif

#ifdef RTCONFIG_VDSL
	 || !strncmp(prefix, "dsl8", 4)
#endif

#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	 || strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
#endif
#ifdef TCSUPPORT_WAN_PTM
	 || !strncmp(prefix, WAN_PTM_EXT_PREFIX, strlen(WAN_PTM_EXT_PREFIX))
#endif
#endif
	) { /* dsl8 ~ dsl8.x for ASUSWRT don't support, WanExt_PVC8e0 ~ WanExt_PVC8e7 for DSL_ASUSWRT don't support */
		*value = strdup("");
		return 0;
	}

#ifdef ASUSWRT
	dsl_svc_cat = nvram_safe_get(strcat_r(prefix, "_svc_cat", tmp));

	if (!strcmp(dsl_svc_cat, "0"))
		snprintf(buf, sizeof(buf), "UBR-npcr");
	else if (!strcmp(dsl_svc_cat, "1"))
		snprintf(buf, sizeof(buf), "UBR");
	else if (!strcmp(dsl_svc_cat, "2"))
		snprintf(buf, sizeof(buf), "CBR");
	else if (!strcmp(dsl_svc_cat, "3"))
		snprintf(buf, sizeof(buf), "VBR-rt");
	else if (!strcmp(dsl_svc_cat, "4"))
		snprintf(buf, sizeof(buf), "GFR");
	else if (!strcmp(dsl_svc_cat, "5"))
		snprintf(buf, sizeof(buf), "VBR-nrt");

#else 	/* DSL_ASUSWRT */
	tcapi_get(prefix, "QOS", dsl_svc_cat);

	if (!strcmp(dsl_svc_cat, "ubr"))
		snprintf(buf, sizeof(buf), "UBR");
	else if (!strcmp(dsl_svc_cat, "cbr"))
		snprintf(buf, sizeof(buf), "CBR");
	else if (!strcmp(dsl_svc_cat, "rt-vbr"))
		snprintf(buf, sizeof(buf), "VBR-rt");
	else if (!strcmp(dsl_svc_cat, "nrt-vbr"))
		snprintf(buf, sizeof(buf), "VBR-nrt");	
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_dsl_atm_pcr(node_t node, char *arg, char *value)
{
	char prefix[sizeof("dslXXXXXXXXXX_")] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
#endif
#ifdef TR098
	char *path = lib_node2path(node->parent);
#endif
#ifdef TR181
	char *path = lib_node2path(node->parent->parent);
#endif	

#ifdef ASUSWRT
	if (atoi(value) < 0 || atoi(value) > 1887)
#else 	/* DSL_ASUSWRT */
	if (atoi(value) < 0 || atoi(value) > 5500)
#endif
		return -1;

	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';
	get_xdsl_prefix(path, prefix);

	/* find unsed index (prefix such as dslX or dsl8.X) if no prefix */
	if (strlen(prefix) == 0)
		find_xdsl_unsed_index(path, prefix);	

	if (strlen(prefix) == 0
#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
	 || strncmp(prefix, "dsl", 3)
#endif

#ifdef RTCONFIG_VDSL
	 || !strncmp(prefix, "dsl8", 4)
#endif

#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	 || strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
#endif
#ifdef TCSUPPORT_WAN_PTM
	 || !strncmp(prefix, WAN_PTM_EXT_PREFIX, strlen(WAN_PTM_EXT_PREFIX))
#endif
#endif
	) /* dsl8 ~ dsl8.x for ASUSWRT don't support, WanExt_PVC8e0 ~ WanExt_PVC8e7 for DSL_ASUSWRT don't support */
		return -1;

#ifdef ASUSWRT
	return set_nvram(strcat_r(prefix, "_pcr", tmp), value);
#else 	/* DSL_ASUSWRT */
	return set_nvram(prefix, "PCR", value);
#endif
}

static int get_dsl_atm_pcr(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
#endif
#ifdef TR098
	char *path = lib_node2path(node->parent);
#endif
#ifdef TR181
	char *path = lib_node2path(node->parent->parent);
#endif	

	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';
	get_xdsl_prefix(path, prefix);

	if (strlen(prefix) == 0
#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
	 || strncmp(prefix, "dsl", 3)
#endif

#ifdef RTCONFIG_VDSL
	 || !strncmp(prefix, "dsl8", 4)
#endif

#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	 || strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
#endif
#ifdef TCSUPPORT_WAN_PTM
	 || !strncmp(prefix, WAN_PTM_EXT_PREFIX, strlen(WAN_PTM_EXT_PREFIX))
#endif
#endif
	) { /* dsl8 ~ dsl8.x for ASUSWRT don't support, WanExt_PVC8e0 ~ WanExt_PVC8e7 for DSL_ASUSWRT don't support */
		*value = strdup("");
		return 0;
	}

#ifdef ASUSWRT
	return get_nvram(strcat_r(prefix, "_pcr", tmp), value);
#else 	/* DSL_ASUSWRT */
	return get_nvram(prefix, "PCR", value);
#endif
}

static int set_dsl_atm_mbs(node_t node, char *arg, char *value)
{
	char prefix[sizeof("dslXXXXXXXXXX_")] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
#endif
#ifdef TR098
	char *path = lib_node2path(node->parent);
#endif
#ifdef TR181
	char *path = lib_node2path(node->parent->parent);
#endif	

#ifdef ASUSWRT
	if (atoi(value) < 0 || atoi(value) > 1887)
#else 	/* DSL_ASUSWRT */
	if (atoi(value) < 0 || atoi(value) > 65535)
#endif
		return -1;

	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';
	get_xdsl_prefix(path, prefix);

	/* find unsed index (prefix such as dslX or dsl8.X) if no prefix */
	if (strlen(prefix) == 0)
		find_xdsl_unsed_index(path, prefix);	

	if (strlen(prefix) == 0
#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
	 || strncmp(prefix, "dsl", 3)
#endif

#ifdef RTCONFIG_VDSL
	 || !strncmp(prefix, "dsl8", 4)
#endif

#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	 || strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
#endif
#ifdef TCSUPPORT_WAN_PTM
	 || !strncmp(prefix, WAN_PTM_EXT_PREFIX, strlen(WAN_PTM_EXT_PREFIX))
#endif
#endif
	) /* dsl8 ~ dsl8.x for ASUSWRT don't support, WanExt_PVC8e0 ~ WanExt_PVC8e7 for DSL_ASUSWRT don't support */
		return -1;

#ifdef ASUSWRT
	return set_nvram(strcat_r(prefix, "_mbs", tmp), value);
#else 	/* DSL_ASUSWRT */
	return set_nvram(prefix, "MBS", value);
#endif
}

static int get_dsl_atm_mbs(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
#endif
#ifdef TR098
	char *path = lib_node2path(node->parent);
#endif
#ifdef TR181
	char *path = lib_node2path(node->parent->parent);
#endif	

	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';
	get_xdsl_prefix(path, prefix);

	if (strlen(prefix) == 0
#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
	 || strncmp(prefix, "dsl", 3)
#endif

#ifdef RTCONFIG_VDSL
	 || !strncmp(prefix, "dsl8", 4)
#endif

#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	 || strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
#endif
#ifdef TCSUPPORT_WAN_PTM
	 || !strncmp(prefix, WAN_PTM_EXT_PREFIX, strlen(WAN_PTM_EXT_PREFIX))
#endif
#endif
	) { /* dsl8 ~ dsl8.x for ASUSWRT don't support, WanExt_PVC8e0 ~ WanExt_PVC8e7 for DSL_ASUSWRT don't support */
		*value = strdup("");
		return 0;
	}

#ifdef ASUSWRT
	return get_nvram(strcat_r(prefix, "_mbs", tmp), value);
#else 	/* DSL_ASUSWRT */
	return get_nvram(prefix, "PCR", value);
#endif
}

static int set_dsl_atm_scr(node_t node, char *arg, char *value)
{
	char prefix[sizeof("dslXXXXXXXXXX_")] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
#endif
#ifdef TR098
	char *path = lib_node2path(node->parent);
#endif
#ifdef TR181
	char *path = lib_node2path(node->parent->parent);
#endif	

#ifdef ASUSWRT
	if (atoi(value) < 0 || atoi(value) > 300)
		return -1;
#endif

	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';
	get_xdsl_prefix(path, prefix);

	/* find unsed index (prefix such as dslX or dsl8.X) if no prefix */
	if (strlen(prefix) == 0)
		find_xdsl_unsed_index(path, prefix);	

	if (strlen(prefix) == 0 || strncmp(prefix, "dsl", 3) || !strncmp(prefix, "dsl8", 4)) /* dsl8 ~ dsl8.x don't support */
		return -1;

#ifdef ASUSWRT
	return set_nvram(strcat_r(prefix, "_scr", tmp), value);
#else 	/* DSL_ASUSWRT */
	if (atoi(value) > tcapi_get_int(prefix, "PCR"))
		return -1;

	return set_nvram(prefix, "SCR", value);
#endif
}

static int get_dsl_atm_scr(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0};
#ifdef ASUSWRT	
	char tmp[32] = {0};
#endif
#ifdef TR098
	char *path = lib_node2path(node->parent);
#endif
#ifdef TR181
	char *path = lib_node2path(node->parent->parent);
#endif	

	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';
	get_xdsl_prefix(path, prefix);

	if (strlen(prefix) == 0
#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
	 || strncmp(prefix, "dsl", 3)
#endif

#ifdef RTCONFIG_VDSL
	 || !strncmp(prefix, "dsl8", 4)
#endif

#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	 || strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
#endif
#ifdef TCSUPPORT_WAN_PTM
	 || !strncmp(prefix, WAN_PTM_EXT_PREFIX, strlen(WAN_PTM_EXT_PREFIX))
#endif
#endif
	) { /* dsl8 ~ dsl8.x for ASUSWRT don't support, WanExt_PVC8e0 ~ WanExt_PVC8e7 for DSL_ASUSWRT don't support */
		*value = strdup("");
		return 0;
	}

#ifdef ASUSWRT
	return get_nvram(strcat_r(prefix, "_scr", tmp), value);
#else 	/* DSL_ASUSWRT */
	return get_nvram(prefix, "SCR", value);
#endif
}

static int set_dsl_enable_dot1q(node_t node, char *arg, char *value)
{
	char prefix[sizeof("dslXXXXXXXXXX_")] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
#endif
	char *path = lib_node2path(node->parent);
	char *value_conv = NULL;

	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';
	value_conv = (!war_strcasecmp(value, "1") || !war_strcasecmp(value, "true")) ? "1" : "0";
	get_xdsl_prefix(path, prefix);

	/* find unsed index (prefix such as dslX or dsl8.X) if no prefix */
	if (strlen(prefix) == 0)
		find_xdsl_unsed_index(path, prefix);	

	if (strlen(prefix) == 0
#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
	 || strncmp(prefix, "dsl", 3)
#endif

#ifdef RTCONFIG_VDSL
	 || !strncmp(prefix, "dsl8", 4)
#endif

#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	 || strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
#endif
#ifdef TCSUPPORT_WAN_PTM
	 || !strncmp(prefix, WAN_PTM_EXT_PREFIX, strlen(WAN_PTM_EXT_PREFIX))
#endif
#endif
	) /* dsl8 ~ dsl8.x for ASUSWRT don't support, WanExt_PVC8e0 ~ WanExt_PVC8e7 for DSL_ASUSWRT don't support */
		return -1;

#ifdef ASUSWRT
	return set_nvram(strcat_r(prefix, "_dot1q", tmp), value_conv);
#else 	/* DSL_ASUSWRT */
	return set_nvram(prefix, "dot1q", !strcmp(value_conv, "1") ? "Yes" : "No");
#endif
}

static int get_dsl_enable_dot1q(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0}, buf[16] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
#endif
	char *path = lib_node2path(node->parent);

	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';
	get_xdsl_prefix(path, prefix);

	if (strlen(prefix) == 0
#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
	 || strncmp(prefix, "dsl", 3)
#endif

#ifdef RTCONFIG_VDSL
	 || !strncmp(prefix, "dsl8", 4)
#endif

#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	 || strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
#endif
#ifdef TCSUPPORT_WAN_PTM
	 || !strncmp(prefix, WAN_PTM_EXT_PREFIX, strlen(WAN_PTM_EXT_PREFIX))
#endif
#endif
	) { /* dsl8 ~ dsl8.x for ASUSWRT don't support, WanExt_PVC8e0 ~ WanExt_PVC8e7 for DSL_ASUSWRT don't support */
		*value = strdup("");
		return 0;
	}

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%s", nvram_match(strcat_r(prefix, "_dot1q", tmp), "1") ? "true" : "false");
#else 	/* DSL_ASUSWRT */
	snprintf(buf, sizeof(buf), "%s", tcapi_match(prefix, "dot1q", "Yes") ? "true" : "false");
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_dsl_dot1q_vid(node_t node, char *arg, char *value)
{
	char prefix[sizeof("dslXXXXXXXXXX_")] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
#endif
	char *path = lib_node2path(node->parent);

#ifdef ASUSWRT
	if (atoi(value) < 0 || atoi(value) > 4095)
		return -1;
#endif

	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';
	get_xdsl_prefix(path, prefix);

	/* find unsed index (prefix such as dslX or dsl8.X) if no prefix */
	if (strlen(prefix) == 0)
		find_xdsl_unsed_index(path, prefix);	

	if (strlen(prefix) == 0
#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
	 || strncmp(prefix, "dsl", 3)
#endif

#ifdef RTCONFIG_VDSL
	 || !strncmp(prefix, "dsl8", 4)
#endif

#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	 || strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
#endif
#ifdef TCSUPPORT_WAN_PTM
	 || !strncmp(prefix, WAN_PTM_EXT_PREFIX, strlen(WAN_PTM_EXT_PREFIX))
#endif
#endif
	) /* dsl8 ~ dsl8.x for ASUSWRT don't support, WanExt_PVC8e0 ~ WanExt_PVC8e7 for DSL_ASUSWRT don't support */
		return -1;

#ifdef ASUSWRT
	return set_nvram(strcat_r(prefix, "_vid", tmp), value);
#else 	/* DSL_ASUSWRT */
	return set_nvram(prefix, "VLANID", value);
#endif
}

static int get_dsl_dot1q_vid(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
#endif
	char *path = lib_node2path(node->parent);

	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';
	get_xdsl_prefix(path, prefix);

	if (strlen(prefix) == 0
#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
	 || strncmp(prefix, "dsl", 3)
#endif

#ifdef RTCONFIG_VDSL
	 || !strncmp(prefix, "dsl8", 4)
#endif

#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	 || strncmp(prefix, WAN_XTM_PREFIX, strlen(WAN_XTM_PREFIX))
#endif
#ifdef TCSUPPORT_WAN_PTM
	 || !strncmp(prefix, WAN_PTM_EXT_PREFIX, strlen(WAN_PTM_EXT_PREFIX))
#endif
#endif
	) { /* dsl8 ~ dsl8.x for ASUSWRT don't support, WanExt_PVC8e0 ~ WanExt_PVC8e7 for DSL_ASUSWRT don't support */
		*value = strdup("");
		return 0;
	}

#ifdef ASUSWRT
	return get_nvram(strcat_r(prefix, "_vid", tmp), value);
#else 	/* DSL_ASUSWRT */
	return get_nvram(prefix, "VLANID", value);
#endif
}
#endif	/* end of RTCONFIG_XDSL */


#ifdef DSL_ASUSWRT	/* start of DSL_ASUSWRT */
static int set_lanhost_dhcpserverenable(node_t node, char *arg, char *value)
{
	return set_nvram(LAN_DHCP_NODE, "type", (!war_strcasecmp(value, "1") || !war_strcasecmp(value, "true")) ? "1" : "0");
}

static int get_lanhost_dhcpserverenable(node_t node, char *arg, char **value)
{
	char buf[8] = {0};

	snprintf(buf, sizeof(buf), "%s", tcapi_match(LAN_DHCP_NODE, "type", "1") ? "true" : "false");

	*value = strdup(buf);
	return *value ? 0 : -1;
}
#endif	/* end of DSL_ASUSWRT */

int write_dhcpstatic_to_nvram(int skip_index)
{
	node_t *children = NULL;
	node_t node;
	int count = 0;
	char buf[64] = {0}, tmp[64] = {0};
#ifdef ASUSWRT
	char dhcp_staticlist[1024] = {0};
#else 	/* DSL_ASUSWRT */
	int i = 0, entry_count = 0;
	char element_name[16] = {0};

	/* clean Dhcpd_Entry for static dhcp first */
	for (i = 0; i < MAX_STATIC_NUM; i ++) {
		memset(element_name, 0, sizeof(element_name));
		snprintf(element_name, sizeof(element_name), "%s%d", STATIC_DHCP_NODE, i);
		tcapi_unset(element_name);
	}
#endif

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	if(lib_resolve_node(LANHOST_DHCPSTATICADDR, &node) == 0) {
#ifdef ASUSWRT
		memset(dhcp_staticlist, 0, sizeof(dhcp_staticlist));
#endif

		count = lib_get_children(node, &children);
		while(count > 0) {
			char name[16] = {0};
			count--;

			if(lib_get_property(children[count], "name", name) == 0) {
				if(string_is_digits(name) == 1 && (skip_index != atoi(name))) {
					char mac[32] = {0}, ip[32] = {0};
#ifdef ASUSWRT					
					char  dhcp_staticlist_rule[64] = {0};

					memset(dhcp_staticlist_rule, 0, sizeof(dhcp_staticlist_rule));
#endif
					/* get the data of Chaddr */
					sprintf(buf, "%s.%s.Chaddr", LANHOST_DHCPSTATICADDR, name);
					sprintf(mac, "%s", __get_parameter_value(buf, tmp));
						
					/* get the data of Yiaddr */
					sprintf(buf, "%s.%s.Yiaddr", LANHOST_DHCPSTATICADDR, name);
					sprintf(ip, "%s", __get_parameter_value(buf, tmp));

#ifdef ASUSWRT
					sprintf(dhcp_staticlist_rule, "<%s>%s", mac, ip);

					if(dhcp_staticlist_rule[0] != '\0')
						strcat(dhcp_staticlist, dhcp_staticlist_rule);
#else 	/* DSL_ASUSWRT */
					memset(element_name, 0, sizeof(element_name));
					snprintf(element_name, sizeof(element_name), "%s%d", STATIC_DHCP_NODE, entry_count);

					tcapi_set(element_name, "IP", ip);
					tcapi_set(element_name, "MAC", mac);
					entry_count++;
#endif
				}
			}
		}

		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}

		//set_nvram("dhcp_staticlist", dhcp_staticlist);
	}
#ifdef ASUSWRT
	set_nvram("dhcp_staticlist", dhcp_staticlist);
#endif
	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

static int del_lanhost_dhcpstatic(node_t node, char *arg, int nin)
{
	char *path = lib_node2path(node);

	tr_log(LOG_DEBUG, "del_lanhost_dhcpstatic: %s.%d", path, nin);

	write_dhcpstatic_to_nvram(nin);
#ifdef ASUSWRT
	nvram_commit();
#else
	tcapi_save();
#endif

	/* Reasign instance name of LANHOST_DHCPSTATICADDR */
	//reasign_instance_name(node, nin, 0);

	return 0;
}

static int set_lanhost_dhcpstatic(node_t node, char *arg, char *value)
{
	return write_dhcpstatic_to_nvram(0);
}


#ifdef TR098	/* start of TR098 */
void update_device_summary()
{
#ifdef DSL_ASUSWRT
#ifdef DSL_N55U_C1
	__set_parameter_value(DEVICE_SUMMARY, "Wireless-N600 Gigabit ADSL Modem Router");
#endif

#ifdef DSL_N66U
	__set_parameter_value(DEVICE_SUMMARY, "Stylish Concurrent Dual-Band Wireless-N900 Gigabit Modem Router");
#endif
#endif
}

void update_deviceInfo_description()
{
   char *name = nvram_safe_get("productid");
   tr_log( LOG_DEBUG, "name= %s", name);
   if(strcmp(name, "RT-N66U") == 0)
       __set_parameter_value(DEVICE_DESCRIPTION, "Dual-Band Wireless-N900 Gigabit Router");
   if(strcmp(name,"RT-AC87U")==0)
       __set_parameter_value(DEVICE_DESCRIPTION, "Dual-Band Wireless-AC2400 Gigabit Router");
   if(strcmp(name,"RT-AC68U")==0)
       __set_parameter_value(DEVICE_DESCRIPTION, "Dual-Band Wireless-AC1900 Gigabit Router");
   if(strcmp(name,"RT-AC66U")==0)
       __set_parameter_value(DEVICE_DESCRIPTION, "802.11ac Dual-Band Wireless-AC1750 Gigabit Router");
   if(strcmp(name,"RT-AC3200")==0)
       __set_parameter_value(DEVICE_DESCRIPTION, "Dual-Band Wireless-AC3200 Gigabit Router");
   if(strcmp(name,"RT-AC1200HP")==0)
       __set_parameter_value(DEVICE_DESCRIPTION, "Dual-Band Wireless-AC1200 Router");
}
int get_wd_inst_by_wan_unit(int wan_unit)
{
	int wd_inst = 0;

#ifdef ASUSWRT
	if (wan_unit == WAN_UNIT_FIRST) {
		if (wan_pri)
			wd_inst = IGD_WANDEVICE_WAN_INST;
		else if (lan_pri)
			wd_inst = IGD_WANDEVICE_LAN_INST;
		else if (usb_pri)
			wd_inst = IGD_WANDEVICE_USB_INST;
	}
	else if (wan_unit == WAN_UNIT_SECOND) {
		if (wan_sec)
			wd_inst = IGD_WANDEVICE_WAN_INST;
		else if (lan_sec)
			wd_inst = IGD_WANDEVICE_LAN_INST;
		else if (usb_sec)
			wd_inst = IGD_WANDEVICE_USB_INST;
	}

#else 	/* DSL_ASUSWRT */
#ifdef TCSUPPORT_WAN_ATM
	if (wan_unit == WAN_ATM_INDEX)
		wd_inst = IGD_WANDEVICE_DSL_INST;
#endif

#ifdef TCSUPPORT_WAN_PTM
	if (wan_unit == WAN_PTM_INDEX)
		wd_inst = IGD_WANDEVICE_DSL_INST;	
#endif

#ifdef TCSUPPORT_WAN_ETHER
	if (wan_unit == WAN_ETHER_INDEX)
		wd_inst = IGD_WANDEVICE_WAN_INST;	
#endif

#ifdef RTCONFIG_USB_MODEM
	if (wan_unit == WAN_USB_INDEX)
		wd_inst = IGD_WANDEVICE_USB_INST;
#endif
#endif

	return wd_inst;
}

int get_wan_unit_by_path(char *path)
{
	int unit = WAN_UNIT_MAX;
	int wandevice_inst = getWANDevInstNum(path);

#ifdef ASUSWRT
	if (pri_wan_inst == wandevice_inst)
		unit = WAN_UNIT_FIRST;
	else if (sec_wan_inst == wandevice_inst)
		unit = WAN_UNIT_SECOND;
#else 	/* DSL_ASUSWRT */

	if (pri_wan_inst == wandevice_inst || sec_wan_inst == wandevice_inst) {
#ifdef RTCONFIG_XDSL
		if (wandevice_inst == IGD_WANDEVICE_DSL_INST) {
#ifdef TCSUPPORT_WAN_ATM
#ifdef TCSUPPORT_WAN_PTM
			if (tcapi_match(WAN_COMMON_NODE, "DSLMode", ADSL_TRANS_MODE))
				unit = WAN_ATM_INDEX;
#else
			unit = WAN_ATM_INDEX;
#endif
#endif

#ifdef TCSUPPORT_WAN_PTM
#ifdef TCSUPPORT_WAN_ATM
			if (tcapi_match(WAN_COMMON_NODE, "DSLMode", VDSL_TRANS_MODE))
				unit = WAN_PTM_INDEX;
#else
			unit = WAN_PTM_INDEX;
#endif
#endif		
		}
#endif

#ifdef TCSUPPORT_WAN_ETHER
		if (wandevice_inst == IGD_WANDEVICE_WAN_INST)
			unit = WAN_ETHER_INDEX;
#endif

#ifdef RTCONFIG_USB_MODEM
		if (wandevice_inst == IGD_WANDEVICE_USB_INST)
			unit = WAN_USB_INDEX;
#endif
	}
#endif
	return unit;
}

int getWANDevInstNum(char *name)
{
	return getInstNum( name, "WANDevice" );
}

int getWANConDevInstNum(char *name)
{
	return getInstNum( name, "WANConnectionDevice" );
}

#if 0
unsigned int getWANPPPConInstNum( char *name )
{
	return getInstNum( name, "WANPPPConnection" );
}

unsigned int getWANIPConInstNum( char *name )
{
	return getInstNum( name, "WANIPConnection" );
}
#endif

char *eth_wanip_prefix_by_path(char *path, char *prefix)
{
	int unit = 0;

	if (strstr(path, IGD_WANDEVICE)) {
#ifdef RTCONFIG_DUALWAN
		unit = get_wan_unit_by_path(path);
#endif

#ifdef RTCONFIG_XDSL
#ifdef ASUSWRT
		if (get_wd_inst_by_wan_unit(unit) == IGD_WANDEVICE_WAN_INST) {
#else 	/* DSL_ASUSWRT */
		if (getWANDevInstNum(path) == IGD_WANDEVICE_DSL_INST) {
#endif
			char buf[256] = {0}, dsl_prefix[16] = {0}, xdsl_internet_prefix[16] = {0};
			int wd_inst = getWANDevInstNum(path);
			int wcd_inst = getWANConDevInstNum(path);

			memset(dsl_prefix, 0, sizeof(dsl_prefix));
			memset(xdsl_internet_prefix, 0, sizeof(xdsl_internet_prefix));

			snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANDSLLinkConfig.X_ASUS_DSLIndex", IGD_WANDEVICE, wd_inst, wcd_inst);
			__get_parameter_value(buf, dsl_prefix);

#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
			if (nvram_match("dslx_transmode", ADSL_TRANS_MODE))
				snprintf(xdsl_internet_prefix, sizeof(xdsl_internet_prefix), "dsl%d", ADSL_INTERNET_INDEX);
#endif 
#ifdef RTCONFIG_VDSL
			if (nvram_match("dslx_transmode", VDSL_TRANS_MODE))
				snprintf(xdsl_internet_prefix, sizeof(xdsl_internet_prefix), "dsl%d", VDSL_INTERNET_INDEX);	
#endif
#else 	/* DSL_ASUSWRT */
#ifdef TCSUPPORT_WAN_ATM
			if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", ADSL_TRANS_MODE)) || !support_adsl_vdsl)
				snprintf(xdsl_internet_prefix, sizeof(xdsl_internet_prefix), "%s%d", WAN_XTM_PREFIX, ADSL_INTERNET_INDEX);
#endif

#ifdef TCSUPPORT_WAN_PTM
			if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", VDSL_TRANS_MODE)) || !support_adsl_vdsl)
				snprintf(xdsl_internet_prefix, sizeof(xdsl_internet_prefix), "%s%d", WAN_XTM_PREFIX, VDSL_INTERNET_INDEX);
#endif
#endif
			
			if (!strcmp(dsl_prefix, xdsl_internet_prefix))
				sprintf(prefix, "wan%d_", unit);
		}
		else
#endif
		{
		sprintf(prefix, "wan%d_", unit);
		}
	}
	else
		sprintf(prefix, "lan_");

	return prefix;	
}

#if 0
int forwarding_instance_exist(char *ip)
{
	node_t *children = NULL;
	node_t node;
	int exist = 0, count = 0;
	char path[128] = {0}, tmp[64] = {0};

	if(lib_resolve_node(L3_FORWARDING, &node) == 0) {
		count = lib_get_children(node, &children);

		while(count > 0) {
			char name[16] = {0};
			count--;

			if(lib_get_property(children[count], "name", name) == 0) {
				if(string_is_digits( name ) == 1) {
					sprintf(path, "%s.%s.DestIPAddress", L3_FORWARDING, name);
					tr_log(LOG_DEBUG, "checking path: %s", path);
					if(!strcmp(__get_parameter_value(path, tmp), ip)) {
						exist = 1;
						break;
					}
				}
			}
		}

		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}
	}
	
	return exist;
}
#endif

char *eth_wlannum_prefix_by_path(char *path, char *prefix)
{
	//static char prefix[] = "wlXXXXXXXXXX_";
	int unit = -1, subunit = -1, index = -1;

	if (strncmp(path, WLAN_2G, strlen(WLAN_2G)) == 0)	// 2.4G
		unit = 0;
	else if (strncmp(path, WLAN_5G, strlen(WLAN_5G)) == 0)	// 5G
		unit = 1;
	else if (strncmp(path, WLAN_CONFIG ".", strlen(WLAN_CONFIG ".")) == 0)
		index = atoi(path + strlen(WLAN_CONFIG "."));

	if (unit < 0) {
		if (index == 1 || index == 2) {			// 2.4G, 5G
			unit = index - 1;
		} else if (index > 2 && index <= 5) {		// 3,4,5 = 2.4G
			subunit = index - 2;
			unit = 0;
		} else if (index > 5 && index <= 8) {		// 6,7,8 = 5G
			subunit = index - 5;
			unit = 1;
		} else
/* TODO: should be an error */
			unit = 0;
	}

	if (subunit < 0)
		sprintf(prefix, "wl%d_", unit);//snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	else
		sprintf(prefix, "wl%d.%d_", unit, subunit);//snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);

	return prefix;
}

int update_wlan_device()
{
	char buf[256] = {0}, tmp[64] = {0}, count_str[8] = {0};
	int wlannum = 0, count = 0, i = 0, bandnum = 0;

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	bandnum = nvram_contains_word("rc_support", "5G") ? 2 : 1;

	snprintf(buf, sizeof(buf), "%s.LANWLANConfigurationNumberOfEntries", LAN_DEVICE);
	/* InternetGatewayDevice.LANDevice.1.LANWLANConfigurationNumberOfEntries */
	count = atoi(__get_parameter_value(buf, tmp));

#ifdef ASUSWRT
	if (bandnum == 1)
		wlannum = bandnum + num_of_mssid_support(0);
	else if (bandnum == 2)
		wlannum = bandnum + num_of_mssid_support(0) + num_of_mssid_support(1);
#else 	/* DSL_ASUSWRT */
	if (bandnum == 1)
		wlannum = bandnum + NUM_MSSID_SUPPORT;
	else if (bandnum == 2)
		wlannum = bandnum + (NUM_MSSID_SUPPORT * 2);
#endif

	if(count == 0 || wlannum != count)
	{
		if(bandnum == 1) {
			sprintf(buf, "%s.2.", WLAN_CONFIG);
			if(delete_object(buf, strlen(buf)) != 0) {
				tr_log(LOG_ERROR, "delete_object failed: %s", buf);
				return -1;
			}
			
			for(i = 6; i < 9; i++) {
				sprintf(buf, "%s.%d.", WLAN_CONFIG, i);
				if(delete_object(buf, strlen(buf)) != 0) {
					tr_log(LOG_ERROR, "delete_object failed: %s", buf);
					return -1;
				}
			}
#ifdef ASUSWRT			
			for(i = 5; i > 2 + num_of_mssid_support(0); i--) 
#else 	/* DSL_ASUSWRT */
			for(i = 5; i > 2 + NUM_MSSID_SUPPORT; i--)
#endif
			{
				sprintf(buf, "%s.%d.", WLAN_CONFIG, i);
				if(delete_object(buf, strlen(buf)) != 0) {
					tr_log(LOG_ERROR, "delete_object failed: %s", buf);
					return -1;
				}
			}
		}
		else if (bandnum == 2) {
#ifdef ASUSWRT
			for(i = 8; i > 5 + num_of_mssid_support(1); i--)
#else 	/* DSL_ASUSWRT */
			for(i = 8; i > 5 + NUM_MSSID_SUPPORT; i--)
#endif
			{
				sprintf(buf, "%s.%d.", WLAN_CONFIG, i);
				if(delete_object(buf, strlen(buf)) != 0) {
					tr_log(LOG_ERROR, "delete_object failed: %s", buf);
					return -1;
				}
			}
		}

		snprintf(buf, sizeof(buf), "%s.LANWLANConfigurationNumberOfEntries", LAN_DEVICE);
		snprintf(count_str, sizeof(count_str), "%d", wlannum);
		__set_parameter_value(buf, count_str);
	}

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);
	return 0;
}

int get_inst_num_by_path(char *path)
{
	node_t node;
	int count = 0, inst_num = 0;
	node_t *children = NULL;

	if(lib_resolve_node(path, &node) == 0) {
		count = lib_get_children(node, &children);
		
		while(count > 0) {
			char name[16] = {0};
			count--;

			if(lib_get_property(children[count], "name", name) == 0) {
				if (string_is_digits(name)) {
					inst_num = atoi(name);
					break;
				}
			}
		}
				
		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}
	}

	return inst_num;
}

int check_ip_ppp_connection(char *prefix, int wd_inst)
{
	char tmp[32] = {0}, buf[256] = {0};
#ifdef ASUSWRT
	char *wan_proto = NULL;
#else 	/* DSL_ASUSWRT */
	char wan_proto[16] = {0};
#endif
	int inst_num = 0;
	node_t node;

#ifdef ASUSWRT	
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
#else
	tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);
#endif

	/* get instance number of WANIPConnection */
	snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnection", IGD_WANDEVICE, wd_inst);
	inst_num = get_inst_num_by_path(buf);

	snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnection.%d", IGD_WANDEVICE, wd_inst, inst_num);
	/* WANIPConnection.X doesn't exist */
	if (lib_resolve_node(buf, &node)) {
		if (wd_inst != IGD_WANDEVICE_USB_INST || !strcmp(wan_proto, "dhcp") || !strcmp(wan_proto, "static")) {
			snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnection.", IGD_WANDEVICE, wd_inst);
			if(add_object(buf, strlen(buf)) >= 9000) {
				tr_log(LOG_ERROR, "add_object failed: %s", buf);
				return -1;
			}

			/* Set the WANIPConnectionNumberOfEntries of WANConnectionDevice */
			snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnectionNumberOfEntries", IGD_WANDEVICE, wd_inst);
			__set_parameter_value(buf, "1");
		}
		else
		{
			/* Set the WANIPConnectionNumberOfEntries of WANConnectionDevice */
			snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnectionNumberOfEntries", IGD_WANDEVICE, wd_inst);
			__set_parameter_value(buf, "0");			
		}
	}
	else
	{
		if (wd_inst == IGD_WANDEVICE_USB_INST && (!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "pptp") || !strcmp(wan_proto, "l2tp"))) {
			/* Delete unsed WANIPConnection for usb */
			snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnection", IGD_WANDEVICE, wd_inst);
			if(delete_all_instance(buf)) {
				tr_log(LOG_ERROR, "delete_all_instance failed: %s", buf);
				return -1;
			}

			/* Set the WANIPConnectionNumberOfEntries of WANIPConnection */
			snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnectionNumberOfEntries", IGD_WANDEVICE, wd_inst);
			__set_parameter_value(buf, "0");	
		}
	}

	if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "pptp") || !strcmp(wan_proto, "l2tp")) {
		/* get instance number of WANPPPConnection */
		snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection", IGD_WANDEVICE, wd_inst);
		inst_num = get_inst_num_by_path(buf);

		snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection.%d", IGD_WANDEVICE, wd_inst, inst_num);
		/* WANPPPConnection.X doesn't exist */
		if (lib_resolve_node(buf, &node)) {
			snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection.", IGD_WANDEVICE, wd_inst);
			if(add_object(buf, strlen(buf)) >= 9000) {
				tr_log(LOG_ERROR, "add_object failed: %s", buf);
				return -1;
			}
		}

		/* Set the WANPPPConnectionNumberOfEntries of WANConnectionDevice */
		snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnectionNumberOfEntries", IGD_WANDEVICE, wd_inst);
		__set_parameter_value(buf, "1");	
	}
	else
	{
		/* Delete unsed WANPPPConnection */
		snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection", IGD_WANDEVICE, wd_inst);
		if(delete_all_instance(buf)) {
			tr_log(LOG_ERROR, "delete_all_instance failed: %s", buf);
			return -1;
		}

		/* Set the WANPPPConnectionNumberOfEntries of WANPPPConnection */
		snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnectionNumberOfEntries", IGD_WANDEVICE, wd_inst);
		__set_parameter_value(buf, "0");
	}

	return 0;	
}

void add_portmapping_entry(char *full_path, int nin, char *path)
{
	node_t node, *children = NULL;
	int add_flag = 0, count = 0;
	char buf[256] = {0}, count_str[8] = {0};
	int inst_num = get_inst_num_by_path(path);

	tr_log(LOG_DEBUG, "%s - %s", __FUNCTION__, path);

	update_flag = 1;	/* avoid to execute the corresponding add/delete/set function */

	memset(buf, 0, sizeof(buf));
	if (inst_num)
		snprintf(buf, sizeof(buf), "%s.%d.PortMapping.", path, inst_num);
			
	if (strncmp(full_path, buf, strlen(full_path))) {
		tr_log(LOG_DEBUG, "%s - The path needs to be added: %s", __FUNCTION__, buf);
		//update_flag = 1;
		tr_log(LOG_DEBUG, "__ao__: %d", __ao__(buf, strlen(buf)));
		//update_flag = 0;
		add_flag = 1;			
	}
	else {
		add_flag = 0;
		tr_log(LOG_DEBUG, "%s - Don't need to add path (%s)", __FUNCTION__, buf);
	}

	/* Reassign instance name of port mapping */
	if (lib_resolve_node(buf, &node) == 0) {
		if (add_flag)
			reasign_instance_name(node, nin, 1);
		else
			reasign_instance_name(node, nin, 0);
	}

	/* Update PortMappingNumberOfEntries of path */
	if (lib_resolve_node(buf, &node) == 0) {
		count = lib_get_children(node, &children);
		if (children) {
			lib_destroy_children(children);
			children = NULL;
		}

		snprintf(buf, sizeof(buf), "%s.%d.PortMappingNumberOfEntries", path, inst_num);
		if (add_flag)
			snprintf(count_str, sizeof(count_str), "%d", count);
		else
			snprintf(count_str, sizeof(count_str), "%d", count + 1);
		__set_parameter_value(buf, count_str);
		tr_log(LOG_DEBUG, "%s - Set %s as %s", __FUNCTION__, buf, count_str);
	}

	update_flag = 0;	/* return to normal */
}

void del_portmapping_entry(char *full_path, int nin, char *path)
{
	node_t node, *children = NULL;
	int delete_flag = 0, count = 0;
	char buf[256] = {0}, count_str[8] = {0};
	int inst_num = get_inst_num_by_path(path);

	tr_log(LOG_DEBUG, "%s - %s", __FUNCTION__, path);

	update_flag = 1;	/* Avoid to execute the corresponding add/delete/set function */

	memset(buf, 0, sizeof(buf));
	if (inst_num)
		snprintf(buf, sizeof(buf), "%s.%d.PortMapping.%d.", path, inst_num, nin);
			
	if (strncmp(full_path, buf, strlen(full_path))) {
		tr_log(LOG_DEBUG, "%s - The path needs to be deleted: %s", __FUNCTION__, buf);
		//update_flag = 1;	/* Avoid to execute the corresponding add/delete/set function */
		tr_log(LOG_DEBUG, "__do__: %d", __do__(buf, strlen(buf)));
		//update_flag = 0;	/* return to normal */
		delete_flag = 1;			
	}
	else {
		delete_flag = 0;
		tr_log(LOG_DEBUG, "%s - Don't need to delete path (%s)", __FUNCTION__, buf);
	}

	/* Reassign instance name of port mapping */
	snprintf(buf, sizeof(buf), "%s.%d.PortMapping.", path, inst_num);
	if(lib_resolve_node(buf, &node) == 0) {
		if (delete_flag)
			reasign_instance_name(node, nin, 1);
		else
			reasign_instance_name(node, nin, 0);
	}

	/* Update PortMappingNumberOfEntries of path */
	if(lib_resolve_node(buf, &node) == 0) {
		count = lib_get_children(node, &children);
		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}

		snprintf(buf, sizeof(buf), "%s.%d.PortMappingNumberOfEntries", path, inst_num);
		if (delete_flag)
			snprintf(count_str, sizeof(count_str), "%d", count);
		else
			snprintf(count_str, sizeof(count_str), "%d", count - 1);
		__set_parameter_value(buf, count_str);
		tr_log(LOG_DEBUG, "%s- Set %s as %s", __FUNCTION__, buf, count_str);
	}

	update_flag = 0;	/* return to normal */
}

void reset_portmapping_count(char *path)
{
	char buf[256] = {0}, count_str[8] = {0}, tmp[32] = {0};
	int inst_num = get_inst_num_by_path(path);
	int count = 0;
	node_t node, *children = NULL;

	if (inst_num) {
		snprintf(buf, sizeof(buf), "%s.%d.PortMappingNumberOfEntries", path, inst_num);
		sprintf(count_str, "%s", __get_parameter_value(buf, tmp));

		snprintf(buf, sizeof(buf), "%s.%d.PortMapping", path, inst_num);
		lib_resolve_node(buf, &node);
		count = lib_get_children(node, &children);
		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}

		if(count != atoi(count_str)) {
			sprintf(count_str, "%d", count);
			snprintf(buf, sizeof(buf), "%s.%d.PortMappingNumberOfEntries", path, inst_num);
			__set_parameter_value(buf, count_str);

			lib_resolve_node(buf, &node);

			if(node->noc == 1 || node->noc == 2) {
				add_inform_parameter(buf, 1);
			}
		}
	}	
}

#ifdef RTCONFIG_XDSL
int check_dsl_wan_device_by_path(char *path)
{
#ifdef RTCONFIG_DUALWAN
	int unit = get_wan_unit_by_path(path);

	if (get_wd_inst_by_wan_unit(unit) == 
#ifdef ASUSWRT
		IGD_WANDEVICE_WAN_INST
#else 	/* DSL_ASUSWRT */
		IGD_WANDEVICE_DSL_INST
#endif
	)
		return 1;
	else
		return 0;
#endif	

	return 1;
}

static int add_dsl_wan_connection_device(node_t node, char *arg, int nin)
{
	char tmp[32] = {0}, buf[256] = {0}, wcd_num[4] = {0};
	char *path = lib_node2path(node->parent);

	path[strlen(path) - 1] = '\0';

	tr_log(LOG_DEBUG, "%s - path=%s, nin=%d", __FUNCTION__, path, nin);

	/* reset the number of WANConnectionDevice */
	snprintf(buf, sizeof(buf), "%s.WANConnectionNumberOfEntries", path);
	snprintf(wcd_num, sizeof(wcd_num), "%d", atoi(__get_parameter_value(buf, tmp)) + 1);
	__set_parameter_value(buf, wcd_num);

	return 0;
}

static int del_dsl_wan_connection_device(node_t node, char *arg, int nin)
{
	char tmp[32] = {0}, prefix[sizeof("dslXXXXXXXXXX")] = {0}, buf[256] = {0}, wcd_num[4] = {0};
	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';

	tr_log(LOG_DEBUG, "%s - path=%s, nin=%d", __FUNCTION__, path, nin);

	/* disable the corresponding dsl when wcd deleted */
	snprintf(buf, sizeof(buf), "%s.%d.WANDSLLinkConfig.X_ASUS_DSLIndex", path, nin);
	__get_parameter_value(buf, prefix);
	if (strlen(prefix))
#ifdef ASUSWRT
		nvram_set(strcat_r(prefix, "_enable", tmp), "0");
#else 	/* DSL_ASUSWRT */
		tcapi_set(prefix, "ACTIVE", "No");
#endif

	/* reset the number of WANConnectionDevice */
	snprintf(buf, sizeof(buf), "%sWANConnectionNumberOfEntries", lib_node2path(node->parent));
	snprintf(wcd_num, sizeof(wcd_num), "%d", atoi(__get_parameter_value(buf, tmp)) - 1);
	__set_parameter_value(buf, wcd_num);

	return 0;
}

static int add_dsl_wanipconnection(node_t node, char *arg, int nin)
{
	char prefix[sizeof("dslXXXXXXXXXX")] = {0}, buf[256] = {0}, xdsl_internet_prefix[sizeof("dslXXXXXXXXXX")] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
#endif
	char *path = lib_node2path(node->parent);
	int unit = 0, wd_inst = 0, inst_num = 0;

	memset(prefix, 0, sizeof(prefix));
	memset(xdsl_internet_prefix, 0, sizeof(xdsl_internet_prefix));
	path[strlen(path) - 1] = '\0';

	tr_log(LOG_DEBUG, "%s - path=%s, nin=%d", __FUNCTION__, path, nin);

	unit = get_wan_unit_by_path(path);
	wd_inst = get_wd_inst_by_wan_unit(unit);

#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
	if (nvram_match("dslx_transmode", ADSL_TRANS_MODE))
		snprintf(xdsl_internet_prefix, sizeof(xdsl_internet_prefix), "dsl%d", ADSL_INTERNET_INDEX);
#endif

#ifdef RTCONFIG_VDSL
	if (nvram_match("dslx_transmode", VDSL_TRANS_MODE))
		snprintf(xdsl_internet_prefix, sizeof(xdsl_internet_prefix), "dsl%d", VDSL_INTERNET_INDEX);	
#endif	//RTCONFIG_VDSL

#else 	/* DSL_ASUSWRT */
#ifdef TCSUPPORT_WAN_ATM
		if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", ADSL_TRANS_MODE)) || !support_adsl_vdsl)
			snprintf(xdsl_internet_prefix, sizeof(xdsl_internet_prefix), "%s%d", WAN_XTM_PREFIX, ADSL_INTERNET_INDEX);
#endif

#ifdef TCSUPPORT_WAN_PTM
		if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", VDSL_TRANS_MODE)) || !support_adsl_vdsl)
			snprintf(xdsl_internet_prefix, sizeof(xdsl_internet_prefix), "%s%d", WAN_XTM_PREFIX, VDSL_INTERNET_INDEX);
#endif
#endif

	snprintf(buf, sizeof(buf), "%s.WANDSLLinkConfig.X_ASUS_DSLIndex", path);
	__get_parameter_value(buf, prefix);

	if (strlen(prefix)) {	/* X_ASUS_DSLIndex info exist */
		if (strcmp(prefix, xdsl_internet_prefix))
			return -1;	/* can't add wanipconnection when prefix isn't dsl0 or dsl8 for internet */

		/* check wherther WANPPPConnection exist or not */
		snprintf(buf, sizeof(buf), "%s.WANPPPConnection", path);
		inst_num = get_inst_num_by_path(buf);
		if (!inst_num) {
			/* set X_ASUS_DSLIndex and dslX_proto */
			//snprintf(buf, sizeof(buf), "%s.WANDSLLinkConfig.X_ASUS_DSLIndex", path);
			//__set_parameter_value(buf, prefix);

#ifdef ASUSWRT
#ifdef RTCONFIG_DSL		
			if (nvram_match("dslx_transmode", ADSL_TRANS_MODE))
				nvram_set(strcat_r(prefix, "_proto", tmp), "ipoa");
#endif

#ifdef RTCONFIG_VDSL
			if (nvram_match("dslx_transmode", VDSL_TRANS_MODE))
				nvram_set(strcat_r(prefix, "_proto", tmp), "dhcp");
#endif

#else 	/* DSL_ASUSWRT */
#ifdef TCSUPPORT_WAN_ATM
			if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", ADSL_TRANS_MODE)) || !support_adsl_vdsl)
				tcapi_set(prefix, "ISP", "0");	/* dhcp */
#endif

#ifdef TCSUPPORT_WAN_PTM
			if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", VDSL_TRANS_MODE)) || !support_adsl_vdsl)
				tcapi_set(prefix, "ISP", "0");	/* dhcp */
#endif
#endif			
		}			
	}
	else 	/* no X_ASUS_DSLIndex info */
	{
		if (find_xdsl_wan_device(wd_inst, xdsl_internet_prefix))
			return -1;	/* can't add wanipconnection when dsl0 or dsl8 exist in other wan device */

		/* set X_ASUS_DSLIndex and dslX_proto for ASUSWRT, Wan_PVC ISP for DSL_ASUSWRT  */
		snprintf(buf, sizeof(buf), "%s.WANDSLLinkConfig.X_ASUS_DSLIndex", path);
		__set_parameter_value(buf, xdsl_internet_prefix);

#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
		if (nvram_match("dslx_transmode", ADSL_TRANS_MODE))
			nvram_set(strcat_r(xdsl_internet_prefix, "_proto", tmp), "ipoa");
#endif

#ifdef RTCONFIG_VDSL
		if (nvram_match("dslx_transmode", VDSL_TRANS_MODE))
			nvram_set(strcat_r(xdsl_internet_prefix, "_proto", tmp), "dhcp");
#endif

#else 	/* DSL_ASUSWRT */
#ifdef TCSUPPORT_WAN_ATM
		if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", ADSL_TRANS_MODE)) || !support_adsl_vdsl)
			tcapi_set(xdsl_internet_prefix, "ISP", "0");	/* dhcp */
#endif

#ifdef TCSUPPORT_WAN_PTM
		if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", VDSL_TRANS_MODE)) || !support_adsl_vdsl)
			tcapi_set(xdsl_internet_prefix, "ISP", "0");	/* dhcp */
#endif
#endif			
	}

	/* update WANIPConnectionNumberOfEntries */
	snprintf(buf, sizeof(buf), "%s.WANIPConnectionNumberOfEntries", path);
	__set_parameter_value(buf, "1");

	return 0;
}

static int del_dsl_wanipconnection(node_t node, char *arg, int nin)
{
	char prefix[sizeof("dslXXXXXXXXXX")] = {0}, buf[256] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
#endif
	char *path = lib_node2path(node->parent);
	int unit = 0, wd_inst = 0, inst_num = 0;
	unsigned int wcd_inst = 0;

	memset(prefix, 0, sizeof(prefix));
	memset(buf, 0, sizeof(buf));
	path[strlen(path) - 1] = '\0';

	tr_log(LOG_DEBUG, "%s - path=%s, nin=%d", __FUNCTION__, path, nin);

	unit = get_wan_unit_by_path(path);
	wd_inst = get_wd_inst_by_wan_unit(unit);
	wcd_inst = getWANConDevInstNum(path);

	/* Delete WANPPPConnection */
	snprintf(buf, sizeof(buf), "%s.WANPPPConnection", path);
	inst_num = get_inst_num_by_path(buf);
	if (inst_num) {
		snprintf(buf, sizeof(buf), "%s.WANPPPConnection.%d.", path, inst_num);
		update_flag = 1;	/* Avoid to execute the corresponding delete function */
		tr_log(LOG_DEBUG, "%s - %s %d", __FUNCTION__, buf, __do__(buf, strlen(buf)));
		update_flag = 0;	/* Return to normal process */

		/* update WANPPPConnectionNumberOfEntries */
		snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnectionNumberOfEntries", IGD_WANDEVICE, wd_inst, wcd_inst);
		tr_log(LOG_DEBUG, "%s - %s %d", __FUNCTION__, buf, __set_parameter_value(buf, "0"));
		//__set_parameter_value(buf, "0");
	}	

	/* update WANIPConnectionNumberOfEntries */
	//snprintf(buf, sizeof(buf), "%s.WANIPConnectionNumberOfEntries", path);
	snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnectionNumberOfEntries", IGD_WANDEVICE, wd_inst, wcd_inst);	
	tr_log(LOG_DEBUG, "%s - %s %d", __FUNCTION__, buf, __set_parameter_value(buf, "0"));
	//__set_parameter_value(buf, "0");

	return 0;
}

static int add_dsl_wanpppconnection(node_t node, char *arg, int nin)
{
	char prefix[sizeof("dslXXXXXXXXXX")] = {0}, buf[256] = {0}, xdsl_internet_prefix[sizeof("dslXXXXXXXXXX")] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
#endif
	char *path = lib_node2path(node->parent);
	int unit = 0, wd_inst = 0;

	memset(prefix, 0, sizeof(prefix));
	memset(xdsl_internet_prefix, 0, sizeof(xdsl_internet_prefix));
	path[strlen(path) - 1] = '\0';

	tr_log(LOG_DEBUG, "%s - path=%s, nin=%d", __FUNCTION__, path, nin);

	unit = get_wan_unit_by_path(path);
	wd_inst = get_wd_inst_by_wan_unit(unit);

#ifdef ASUSWRT
#ifdef RTCONFIG_DSL
	if (nvram_match("dslx_transmode", ADSL_TRANS_MODE))
		snprintf(xdsl_internet_prefix, sizeof(xdsl_internet_prefix), "dsl%d", ADSL_INTERNET_INDEX);
#endif

#ifdef RTCONFIG_VDSL
	if (nvram_match("dslx_transmode", VDSL_TRANS_MODE))
		snprintf(xdsl_internet_prefix, sizeof(xdsl_internet_prefix), "dsl%d", VDSL_INTERNET_INDEX);	
#endif

#else 	/* DSL_ASUSWRT */
#ifdef TCSUPPORT_WAN_ATM
	if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", ADSL_TRANS_MODE)) || !support_adsl_vdsl)
		snprintf(xdsl_internet_prefix, sizeof(xdsl_internet_prefix), "%s%d", WAN_XTM_PREFIX, ADSL_INTERNET_INDEX);
#endif

#ifdef TCSUPPORT_WAN_PTM
	if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", VDSL_TRANS_MODE)) || !support_adsl_vdsl)
		snprintf(xdsl_internet_prefix, sizeof(xdsl_internet_prefix), "%s%d", WAN_XTM_PREFIX, VDSL_INTERNET_INDEX);
#endif
#endif	

	snprintf(buf, sizeof(buf), "%s.WANDSLLinkConfig.X_ASUS_DSLIndex", path);
	__get_parameter_value(buf, prefix);

	if (strlen(prefix)) {	/* X_ASUS_DSLIndex info exist */
		if (strcmp(prefix, xdsl_internet_prefix))
			return -1;	/* can't add wanpppconnection when prefix isn't dsl0 or dsl8 for internet */

		//snprintf(buf, sizeof(buf), "%s.WANDSLLinkConfig.X_ASUS_DSLIndex", path);
		//__set_parameter_value(buf, prefix);
#ifdef ASUSWRT
		nvram_set(strcat_r(prefix, "_proto", tmp), "pppoe");
#else 	/* DSL_ASUSWRT */
		tcapi_set(prefix, "ISP", "2");		/* pppoe/pppoa */
#endif
	}
	else 	/* no X_ASUS_DSLIndex info */
	{
		if (find_xdsl_wan_device(wd_inst, xdsl_internet_prefix))
			return -1;	/* can't add wanpppconnection when dsl0 or dsl8 exist in other wan device */

		/* set X_ASUS_DSLIndex and dslX_proto */
		snprintf(buf, sizeof(buf), "%s.WANDSLLinkConfig.X_ASUS_DSLIndex", path);
		__set_parameter_value(buf, xdsl_internet_prefix);
#ifdef ASUSWRT
		nvram_set(strcat_r(xdsl_internet_prefix, "_proto", tmp), "pppoe");
#else 	/* DSL_ASUSWRT */
		tcapi_set(prefix, "ISP", "2");		/* pppoe/pppoa */
#endif
	}

	/* update WANPPPConnectionNumberOfEntries */
	snprintf(buf, sizeof(buf), "%s.WANPPPConnectionNumberOfEntries", path);
	__set_parameter_value(buf, "1");

	return 0;
}

static int del_dsl_wanpppconnection(node_t node, char *arg, int nin)
{
	char prefix[sizeof("dslXXXXXXXXXX")] = {0}, buf[256] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
#endif
	char *path = lib_node2path(node->parent);
	int inst_num = 0;

	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';

	tr_log(LOG_DEBUG, "%s - path=%s, nin=%d", __FUNCTION__, path, nin);

	snprintf(buf, sizeof(buf), "%s.WANDSLLinkConfig.X_ASUS_DSLIndex", path);
	__get_parameter_value(buf, prefix);

	if (strlen(prefix))	{ /* X_ASUS_DSLIndex info exist */
		snprintf(buf, sizeof(buf), "%s.WANIPConnection", path);
		inst_num = get_inst_num_by_path(buf);
		if (inst_num)
#ifdef ASUSWRT
			nvram_set(strcat_r(prefix, "_proto", tmp), "ipoa");
#else 	/* DSL_ASUSWRT */
			tcapi_set(prefix, "ISP", "0");	/* dhcp */
#endif
		else
#ifdef ASUSWRT
			nvram_set(strcat_r(prefix, "_proto", tmp), "bridge");
#else 	/* DSL_ASUSWRT */
			tcapi_set(prefix, "ISP", "3");	/* bridge */
#endif
	}

	/* update WANPPPConnectionNumberOfEntries */
	snprintf(buf, sizeof(buf), "%s.WANPPPConnectionNumberOfEntries", path);
	__set_parameter_value(buf, "0");

	return 0;
}

int find_xdsl_wan_device(int wd_inst, char *index_name)
{
	char buf[256] = {0};
	node_t *children = NULL;
	int count = 0;
	int match_wcd_inst = 0;
	node_t node;

	snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice", IGD_WANDEVICE, wd_inst);

	if (lib_resolve_node(buf, &node) == 0) {
		count = lib_get_children(node, &children);
		
		while (count > 0) {
			char name[8] = {0};
			char dsl_index[16] = {0};

			count--;
			if (lib_get_property(children[count], "name", name) == 0) {
				if (string_is_digits(name)) {
					snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%s.WANDSLLinkConfig.X_ASUS_DSLIndex", IGD_WANDEVICE, wd_inst, name);
					__get_parameter_value(buf, dsl_index);

					if (!strcmp(index_name, dsl_index))	{ /* find it */
						match_wcd_inst = atoi(name);
						break;
					}
				}
			}
		}
				
		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}
	}

	return match_wcd_inst;
}

#if defined(RTCONFIG_DSL) || defined(TCSUPPORT_WAN_ATM)
int add_adsl_wan_device(int wd_inst, char *prefix, int i)
{
	int inst_index = 0;
	int result = 0;
	int inst_num = 0;
	char buf[256] = {0};
#ifdef ASUSWRT
	char *dsl_proto = NULL;
	char tmp[32] = {0};
#else 	/* DSL_ASUSWRT */
	char dsl_proto[16] = {0};
#endif

	snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.", IGD_WANDEVICE, wd_inst);
	if ((inst_index = add_object(buf, strlen(buf))) < 9000) {
		snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANDSLLinkConfig.X_ASUS_DSLIndex", IGD_WANDEVICE, wd_inst, inst_index);
		__set_parameter_value(buf, prefix);
		
		if (i == 0) {	/* WANConnectionDevice for internet */
#ifdef RTCONFIG_DSL
			dsl_proto = nvram_safe_get(strcat_r(prefix, "_proto", tmp));
			/* create WANIPConnection for pppoe, pppoa, ipoa, mer */
			if (!strcmp(dsl_proto, "pppoe") || !strcmp(dsl_proto, "pppoa") || 
				!strcmp(dsl_proto, "ipoa") || !strcmp(dsl_proto, "mer")) {
#endif
#ifdef TCSUPPORT_WAN_ATM
			tcapi_get(prefix, "ISP", dsl_proto);
			/* create WANIPConnection for dynamic, static */
			if (!strcmp(dsl_proto, "0") || !strcmp(dsl_proto, "1")) {
#endif

				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnection.", IGD_WANDEVICE, wd_inst, inst_index);
				if(add_object(buf, strlen(buf)) >= 9000) {
					tr_log(LOG_ERROR, "add_object failed: %s", buf);
					return result;
				}

				/* set the WANIPConnectionNumberOfEntries of WANIPConnection */
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnectionNumberOfEntries", IGD_WANDEVICE, wd_inst, inst_index);
				__set_parameter_value(buf, "1");
			}

#ifdef RTCONFIG_DSL
			/* create WANPPPConnection for pppoe, pppoa */
			if (!strcmp(dsl_proto, "pppoe") || !strcmp(dsl_proto, "pppoa")) {
#endif
#ifdef TCSUPPORT_WAN_ATM
			/* create WANPPPConnection for pppoe */
			if (!strcmp(dsl_proto, "2")) {
#endif

				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnection.", IGD_WANDEVICE, wd_inst, inst_index);
				if(add_object(buf, strlen(buf)) >= 9000) {
					tr_log(LOG_ERROR, "add_object failed: %s", buf);
					return result;
				}

				/* set the WANIPConnectionNumberOfEntries of WANIPConnection */
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnectionNumberOfEntries", IGD_WANDEVICE, wd_inst, inst_index);
				__set_parameter_value(buf, "1");
			}

			/* set static inform for ExternalIPAddress of WANIPConnection/WANPPPConnection */
#ifdef RTCONFIG_DSL
			if (nvram_match("dslx_transmode", ADSL_TRANS_MODE)) {
				if (!strcmp(dsl_proto, "ipoa") || !strcmp(dsl_proto, "mer")) {
#endif
#ifdef TCSUPPORT_WAN_ATM
#ifdef TCSUPPORT_WAN_PTM
			if (tcapi_match(WAN_COMMON_NODE, "DSLMode", ADSL_TRANS_MODE)) {
#else
			{
#endif
				if (!strcmp(dsl_proto, "0") || !strcmp(dsl_proto, "1")) {
#endif
					snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnection", IGD_WANDEVICE, wd_inst, inst_index);
					inst_num = get_inst_num_by_path(buf);

					if (inst_num) {
						snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnection.%d.ExternalIPAddress", IGD_WANDEVICE, wd_inst, inst_index, inst_num);
						add_inform_parameter(buf, 0);
					}
				}
#ifdef RTCONFIG_DSL
				else if (!strcmp(dsl_proto, "pppoe") || !strcmp(dsl_proto, "pppoa")) {
#endif
#ifdef TCSUPPORT_WAN_ATM
				else if (!strcmp(dsl_proto, "2")) {
#endif					
					snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnection", IGD_WANDEVICE, wd_inst, inst_index);
					inst_num = get_inst_num_by_path(buf);

					if (inst_num) {
						snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnection.%d.ExternalIPAddress", IGD_WANDEVICE, wd_inst, inst_index, inst_num);
						add_inform_parameter(buf, 0);
					}
				}
			}
		}

		result = 1;
	}
	else
		tr_log(LOG_DEBUG, "%s - fail to add object (%s)", __FUNCTION__, buf);

	return result;
}
#endif

#if defined(RTCONFIG_VDSL) || defined(TCSUPPORT_WAN_PTM)
int add_vdsl_wan_device(int wd_inst, char *prefix, int i)
{
	int inst_index = 0;
	int result = 0;
	int inst_num = 0;
	char buf[256] = {0};
#ifdef ASUSWRT
	char *dsl_proto = NULL;
#else 	/* DSL_ASUSWRT */
	char dsl_proto[16] = {0};	
#endif
	char tmp[32] = {0};

	snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.", IGD_WANDEVICE, wd_inst);
	if ((inst_index = add_object(buf, strlen(buf))) < 9000) {
		snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANDSLLinkConfig.X_ASUS_DSLIndex", IGD_WANDEVICE, wd_inst, inst_index);
		__set_parameter_value(buf, prefix);

		if (i == 0) {	/* WANConnectionDevice for internet */
#ifdef RTCONFIG_VDSL
			dsl_proto = nvram_safe_get(strcat_r(prefix, "_proto", tmp));
						
			/* create WANIPConnection for dhcp, static, pppoe */
			if (!strcmp(dsl_proto, "pppoe") || !strcmp(dsl_proto, "dhcp") || !strcmp(dsl_proto, "static")) {
#endif
#ifdef TCSUPPORT_WAN_PTM
			tcapi_get(prefix, "ISP", dsl_proto);
			/* create WANIPConnection for dynamic, static */
			if (!strcmp(dsl_proto, "0") || !strcmp(dsl_proto, "1")) {
#endif
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnection.", IGD_WANDEVICE, wd_inst, inst_index);
				if(add_object(buf, strlen(buf)) >= 9000) {
					tr_log(LOG_ERROR, "add_object failed: %s", buf);
					return result;
				}

				/* set the WANIPConnectionNumberOfEntries of WANIPConnection */
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnectionNumberOfEntries", IGD_WANDEVICE, wd_inst, inst_index);
				__set_parameter_value(buf, "1");
			}

#ifdef RTCONFIG_VDSL
			/* create WANPPPConnection for pppoe */
			if (!strcmp(dsl_proto, "pppoe")) {
#endif
#ifdef TCSUPPORT_WAN_PTM
			/* create WANPPPConnection for pppoe */
			if (!strcmp(dsl_proto, "2")) {
#endif

				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnection.", IGD_WANDEVICE, wd_inst, inst_index);
				if(add_object(buf, strlen(buf)) >= 9000) {
					tr_log(LOG_ERROR, "add_object failed: %s", buf);
					return result;
				}

				/* set the WANIPConnectionNumberOfEntries of WANIPConnection */
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnectionNumberOfEntries", IGD_WANDEVICE, wd_inst, inst_index);
				__set_parameter_value(buf, "1");
			}

			/* set static inform for ExternalIPAddress of WANIPConnection/WANPPPConnection */
#ifdef RTCONFIG_VDSL
			if (nvram_match("dslx_transmode", VDSL_TRANS_MODE)) {
				if (!strcmp(dsl_proto, "dhcp") || !strcmp(dsl_proto, "static")) {
#endif
#ifdef TCSUPPORT_WAN_PTM
#ifdef TCSUPPORT_WAN_ATM
			if (tcapi_match(WAN_COMMON_NODE, "DSLMode", VDSL_TRANS_MODE)) {
#else
			{
#endif
				if (!strcmp(dsl_proto, "0") || !strcmp(dsl_proto, "1")) {
#endif
					snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnection", IGD_WANDEVICE, wd_inst, inst_index);
					inst_num = get_inst_num_by_path(buf);

					if (inst_num) {
						snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnection.%d.ExternalIPAddress", IGD_WANDEVICE, wd_inst, inst_index, inst_num);
						add_inform_parameter(buf, 0);
					}
				}
#ifdef RTCONFIG_VDSL
				else if (!strcmp(dsl_proto, "pppoe")) {
#endif
#ifdef TCSUPPORT_WAN_PTM
				else if (!strcmp(dsl_proto, "2")) {
#endif
					snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnection", IGD_WANDEVICE, wd_inst, inst_index);
					inst_num = get_inst_num_by_path(buf);

					if (inst_num) {
						snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnection.%d.ExternalIPAddress", IGD_WANDEVICE, wd_inst, inst_index, inst_num);
						add_inform_parameter(buf, 0);
					}
				}
			}
		}

		result = 1;
	}
	else
		tr_log(LOG_DEBUG, "%s - fail to add object (%s)", __FUNCTION__, buf);

	return result;
}
#endif

int update_xdsl_wan_device(int wd_inst)
{
	char buf[256] = {0};
	node_t *children = NULL;
	int wcd_count = 0;
	node_t node;
	char prefix[sizeof("dslXXXXXXXXXX_")] = {0};
	int i = 0;
	char count_str[4] = {0};
#ifdef ASUSWRT
	char *dsl_proto = NULL;
	char tmp[32] = {0};
#else 	/* DSL_ASUSWRT */
	char dsl_proto[16] = {0};
#endif
	int wcd_inst = 0;
	int inst_num = 0;
		
	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	/* count the number of InternetGatewayDevice.WANDevice.X.WANConnectionDevice */
	snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice", IGD_WANDEVICE, wd_inst);
	if (lib_resolve_node(buf, &node) == 0) {
		wcd_count = lib_get_children(node, &children);
		if (children) {
			lib_destroy_children(children);
			children = NULL;
		}
	}
	else
	{
		tr_log(LOG_DEBUG, "%s - can't count the number of WANConnectionDevice", __FUNCTION__);
		return -1;
	}

	if (wcd_count == 0) {
#if defined(RTCONFIG_DSL) || defined(TCSUPPORT_WAN_ATM)
		/* create WANConnectionDevice for ADSL (ATM) */
		for (i = 0; i < DSL_NUM; i++) {
#ifdef RTCONFIG_DSL
			snprintf(prefix, sizeof(prefix), "dsl%d", i);
			if (nvram_match(strcat_r(prefix, "_enable", tmp), "1")) {
#endif
#ifdef TCSUPPORT_WAN_ATM
			snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, i);
			if (tcapi_match(prefix, "Active", "Yes")) {
#endif
			
				if (add_adsl_wan_device(wd_inst, prefix, i))
					wcd_count++;
			}
		}
#endif

#if defined(RTCONFIG_VDSL) || defined(TCSUPPORT_WAN_PTM)
		/* create WANConnectionDevice for VDSL (PTM) */
		for (i = 0; i < DSL_NUM; i++) {
#ifdef RTCONFIG_VDSL
			if (i == 0)
				snprintf(prefix, sizeof(prefix), "dsl8");
			else
				snprintf(prefix, sizeof(prefix), "dsl8.%d", i);

			if (nvram_match(strcat_r(prefix, "_enable", tmp), "1"))
#endif
#ifdef TCSUPPORT_WAN_PTM
			if (i == 0)
				snprintf(prefix, sizeof(prefix), "%s8", WAN_XTM_PREFIX);
			else	
				snprintf(prefix, sizeof(prefix), "%s%d", WAN_PTM_EXT_PREFIX, i - 1);
			if (tcapi_match(prefix, "Active", "Yes"))
#endif
			{
				if (add_vdsl_wan_device(wd_inst, prefix, i))
					wcd_count++;
			}
		}
#endif
	}
	else
	{
#if defined(RTCONFIG_DSL) || defined(TCSUPPORT_WAN_ATM)
		/* check whether add or delete WANConnectionDevice for ADSL (ATM) */
		for (i = 0; i < DSL_NUM; i++) {
#ifdef RTCONFIG_DSL
			snprintf(prefix, sizeof(prefix), "dsl%d", i);
			if (nvram_match(strcat_r(prefix, "_enable", tmp), "1"))
#endif
#ifdef TCSUPPORT_WAN_ATM
			snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, i);
			if (tcapi_match(prefix, "Active", "Yes"))
#endif
			{
				if (!find_xdsl_wan_device(wd_inst, prefix)) {
					if (add_adsl_wan_device(wd_inst, prefix, i))
						wcd_count++;
				}
			}			
		}
#endif

#if defined(RTCONFIG_VDSL) || defined(TCSUPPORT_WAN_PTM)
		/* check whether add or delete WANConnectionDevice for VDSL (PTM) */
		for (i = 0; i < DSL_NUM; i++) {
#ifdef RTCONFIG_VDSL
			if (i == 0)
				snprintf(prefix, sizeof(prefix), "dsl8");
			else
				snprintf(prefix, sizeof(prefix), "dsl8.%d", i);

			if (nvram_match(strcat_r(prefix, "_enable", tmp), "1"))
#endif
#ifdef TCSUPPORT_WAN_PTM
			if (i == 0)
				snprintf(prefix, sizeof(prefix), "%s8", WAN_XTM_PREFIX);
			else	
				snprintf(prefix, sizeof(prefix), "%s%d", WAN_PTM_EXT_PREFIX, i - 1);
			if (tcapi_match(prefix, "Active", "Yes"))
#endif
			{
				if (!find_xdsl_wan_device(wd_inst, prefix)) {
					if (add_vdsl_wan_device(wd_inst, prefix, i))
						wcd_count++;
				}
			}			
		}
#endif
	}

	/* set InternetGatewayDevice.WANDevice.X.WANConnectionNumberOfEntries */
	snprintf(buf, sizeof(buf), "%s.%d.WANConnectionNumberOfEntries", IGD_WANDEVICE, wd_inst);
	snprintf(count_str, sizeof(count_str), "%d", wcd_count);
	__set_parameter_value(buf, count_str);

#if defined(RTCONFIG_DSL) || defined(TCSUPPORT_WAN_ATM)
	/* set static inform for ExternalIPAddress of WANIPConnection/WANPPPConnection */
#ifdef RTCONFIG_DSL
	if (nvram_match("dslx_transmode", ADSL_TRANS_MODE))
		snprintf(prefix, sizeof(prefix), "dsl%d", ADSL_INTERNET_INDEX);
#endif
#ifdef TCSUPPORT_WAN_ATM
#ifdef TCSUPPORT_WAN_PTM
	if (tcapi_match(WAN_COMMON_NODE, "DSLMode", ADSL_TRANS_MODE))
#endif
	{
		snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, ADSL_INTERNET_INDEX);
#endif
		
		if ((wcd_inst = find_xdsl_wan_device(wd_inst, prefix))) {
#ifdef RTCONFIG_DSL
			dsl_proto = nvram_safe_get(strcat_r(prefix, "_proto", tmp));
			if (!strcmp(dsl_proto, "ipoa") || !strcmp(dsl_proto, "mer"))
#endif
#ifdef TCSUPPORT_WAN_ATM
			tcapi_get(prefix, "ISP", dsl_proto);
			if (!strcmp(dsl_proto, "0") || !strcmp(dsl_proto, "1"))
#endif
			{
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnection", IGD_WANDEVICE, wd_inst, wcd_inst);
				inst_num = get_inst_num_by_path(buf);

				if (inst_num) {
					snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnection.%d.ExternalIPAddress", IGD_WANDEVICE, wd_inst, wcd_inst, inst_num);
					add_inform_parameter(buf, 0);
				}
			}
#ifdef RTCONFIG_DSL
			else if (!strcmp(dsl_proto, "pppoe") || !strcmp(dsl_proto, "pppoa"))
#endif
#ifdef TCSUPPORT_WAN_ATM
			else if (!strcmp(dsl_proto, "2"))
#endif			
			{
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnection", IGD_WANDEVICE, wd_inst, wcd_inst);
				inst_num = get_inst_num_by_path(buf);

				if (inst_num) {
					snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnection.%d.ExternalIPAddress", IGD_WANDEVICE, wd_inst, wcd_inst, inst_num);
					add_inform_parameter(buf, 0);
				}
			}			
		}
	}
#endif

#if defined(RTCONFIG_VDSL) || defined(TCSUPPORT_WAN_PTM) 
#ifdef RTCONFIG_DSL
	if (nvram_match("dslx_transmode", VDSL_TRANS_MODE)) {
		snprintf(prefix, sizeof(prefix), "dsl%d", VDSL_INTERNET_INDEX);
#endif
#ifdef TCSUPPORT_WAN_PTM
#ifdef TCSUPPORT_WAN_ATM
	if (tcapi_match(WAN_COMMON_NODE, "DSLMode", VDSL_TRANS_MODE))
#endif
	{
		snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, VDSL_INTERNET_INDEX);
#endif
		if ((wcd_inst = find_xdsl_wan_device(wd_inst, prefix))) {
#ifdef RTCONFIG_VDSL
			dsl_proto = nvram_safe_get(strcat_r(prefix, "_proto", tmp));
			if (!strcmp(dsl_proto, "dhcp") || !strcmp(dsl_proto, "static"))
#endif
#ifdef TCSUPPORT_WAN_ATM
			tcapi_get(prefix, "ISP", dsl_proto);
			if (!strcmp(dsl_proto, "0") || !strcmp(dsl_proto, "1"))
#endif
			{
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnection", IGD_WANDEVICE, wd_inst, wcd_inst);
				inst_num = get_inst_num_by_path(buf);

				if (inst_num) {
					snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnection.%d.ExternalIPAddress", IGD_WANDEVICE, wd_inst, wcd_inst, inst_num);
					add_inform_parameter(buf, 0);
				}
			}
#ifdef RTCONFIG_VDSL
			else if (!strcmp(dsl_proto, "pppoe"))
#endif
#ifdef TCSUPPORT_WAN_ATM
			else if (!strcmp(dsl_proto, "2"))
#endif		
			{
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnection", IGD_WANDEVICE, wd_inst, wcd_inst);
				inst_num = get_inst_num_by_path(buf);

				if (inst_num) {
					snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnection.%d.ExternalIPAddress", IGD_WANDEVICE, wd_inst, wcd_inst, inst_num);
					add_inform_parameter(buf, 0);
				}
			}
		}
	}
#endif

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

void xdsl_compare_portmapping_instance(int unit, int wd_inst, char *desc, 
	char *src_addr, char *ext_port, char *in_client, char *in_port, char *proto)
{
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *dsl_proto = NULL;
#else 	/* DSL_ASUSWRT */
	char dsl_proto[8] = {0}, dsl_prefix[sizeof("wanXXXXXXXXXX_")] = {0};
#endif
	char buf[256]= {0};
	int wcd_inst = 0, inst_num = 0;
	int xdsl_internet_index[] = {ADSL_INTERNET_INDEX
#if defined(RTCONFIG_VDSL) || defined(TCSUPPORT_WAN_PTM)
								, VDSL_INTERNET_INDEX
#endif
								};
	int i = 0;

	for (i = 0; i < sizeof(xdsl_internet_index)/sizeof(int); i ++) {
#ifdef ASUSWRT
		snprintf(prefix, sizeof(prefix), "dsl%d", xdsl_internet_index[i]);
#else 	/* DSL_ASUSWRT */
#ifdef TCSUPPORT_WAN_ATM
		if (xdsl_internet_index[i] == WAN_ATM_INDEX)
			snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, WAN_ATM_INDEX);
#endif
#ifdef TCSUPPORT_WAN_PTM
		if (xdsl_internet_index[i] == WAN_PTM_INDEX)
			snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, WAN_PTM_INDEX);
#endif
#endif
		if ((wcd_inst = find_xdsl_wan_device(wd_inst, prefix))) {
#ifdef ASUSWRT
			dsl_proto = nvram_safe_get(strcat_r(prefix, "_proto", tmp));
#else 	/* DSL_ASUSWRT */
			snprintf(dsl_prefix, sizeof(dsl_prefix), "wan%d", xdsl_internet_index[i]);
			tcapi_get(WANDUCK_NODE, strcat_r(dsl_prefix, "_proto", tmp), dsl_proto);
#endif
			snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnection", IGD_WANDEVICE, wd_inst, wcd_inst);
			inst_num = get_inst_num_by_path(buf);

			if (inst_num) {
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnection.%d.PortMapping", IGD_WANDEVICE, wd_inst, wcd_inst, inst_num);
				compare_portmapping_instance(buf, desc, src_addr, ext_port, in_client, in_port, proto);
			}

			if (!strcmp(dsl_proto, "pppoe") || !strcmp(dsl_proto, "pppoa")) {
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnection", IGD_WANDEVICE, wd_inst, wcd_inst);
				inst_num = get_inst_num_by_path(buf);

				if (inst_num) {
					snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnection.%d.PortMapping", IGD_WANDEVICE, wd_inst, wcd_inst, inst_num);
					compare_portmapping_instance(buf, desc, src_addr, ext_port, in_client, in_port, proto);
				}
			}
		}
	}
}

char *xdsl_pick_portmapping_instance(int unit, int wd_inst, char *path)
{
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *dsl_proto = NULL;
#else 	/* DSL_ASUSWRT */
	char dsl_proto[8] = {0}, dsl_prefix[sizeof("wanXXXXXXXXXX_")] = {0};
#endif
	char buf[256]= {0};
	int wcd_inst = 0, inst_num = 0;
	int xdsl_internet_index[] = {ADSL_INTERNET_INDEX
#ifdef RTCONFIG_VDSL
								, VDSL_INTERNET_INDEX
#endif
								};
	int i = 0;

	for (i = 0; i < sizeof(xdsl_internet_index)/sizeof(int); i ++) {
#ifdef ASUSWRT
		snprintf(prefix, sizeof(prefix), "dsl%d", xdsl_internet_index[i]);
#else 	/* DSL_ASUSWRT */
#ifdef TCSUPPORT_WAN_ATM
		if (xdsl_internet_index[i] == WAN_ATM_INDEX)
			snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, WAN_ATM_INDEX);
#endif
#ifdef TCSUPPORT_WAN_PTM
		if (xdsl_internet_index[i] == WAN_PTM_INDEX)
			snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, WAN_PTM_INDEX);
#endif
#endif
		if ((wcd_inst = find_xdsl_wan_device(wd_inst, prefix))) {
#ifdef ASUSWRT
			dsl_proto = nvram_safe_get(strcat_r(prefix, "_proto", tmp));
#else 	/* DSL_ASUSWRT */
			snprintf(dsl_prefix, sizeof(dsl_prefix), "wan%d", xdsl_internet_index[i]);
			tcapi_get(WANDUCK_NODE, strcat_r(dsl_prefix, "_proto", tmp), dsl_proto);
#endif

			snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnection", IGD_WANDEVICE, wd_inst, wcd_inst);
			inst_num = get_inst_num_by_path(buf);

			if (inst_num) {
				sprintf(path, "%s.%d.WANConnectionDevice.%d.WANIPConnection.%d.PortMapping", IGD_WANDEVICE, wd_inst, wcd_inst, inst_num);
				break;
			}

			if (!strcmp(dsl_proto, "pppoe") || !strcmp(dsl_proto, "pppoa")) {
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnection", IGD_WANDEVICE, wd_inst, wcd_inst);
				inst_num = get_inst_num_by_path(buf);

				if (inst_num) {
					sprintf(path, "%s.%d.WANConnectionDevice.%d.WANPPPConnection.%d.PortMapping", IGD_WANDEVICE, wd_inst, wcd_inst, inst_num);
					break;
				}
			}
		}
	}

	return path;
}

void xdsl_compare_portmapping_nvram(int unit, int wd_inst, char *ext_port, 
	char *in_port, char *proto, char *src_addr, char *in_client, char *desc, int index)
{
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *dsl_proto = NULL;
#else 	/* DSL_ASUSWRT */
	char dsl_proto[8] = {0}, dsl_prefix[sizeof("wanXXXXXXXXXX_")] = {0};
#endif
	char buf[256]= {0};
	int wcd_inst = 0, inst_num = 0;
	int xdsl_internet_index[] = {ADSL_INTERNET_INDEX
#if defined(RTCONFIG_VDSL) || defined(TCSUPPORT_WAN_PTM)
								, VDSL_INTERNET_INDEX
#endif
								};
	int i = 0;

	for (i = 0; i < sizeof(xdsl_internet_index)/sizeof(int); i ++) {
#ifdef ASUSWRT
		snprintf(prefix, sizeof(prefix), "dsl%d", xdsl_internet_index[i]);
#else 	/* DSL_ASUSWRT */
#ifdef TCSUPPORT_WAN_ATM
		if (xdsl_internet_index[i] == WAN_ATM_INDEX)
			snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, WAN_ATM_INDEX);
#endif
#ifdef TCSUPPORT_WAN_PTM
		if (xdsl_internet_index[i] == WAN_PTM_INDEX)
			snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, WAN_PTM_INDEX);
#endif
#endif
		if ((wcd_inst = find_xdsl_wan_device(wd_inst, prefix))) {
#ifdef ASUSWRT
			dsl_proto = nvram_safe_get(strcat_r(prefix, "_proto", tmp));
#else 	/* DSL_ASUSWRT */
			snprintf(dsl_prefix, sizeof(dsl_prefix), "wan%d", xdsl_internet_index[i]);
			tcapi_get(WANDUCK_NODE, strcat_r(dsl_prefix, "_proto", tmp), dsl_proto);
#endif
			snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnection", IGD_WANDEVICE, wd_inst, wcd_inst);
			inst_num = get_inst_num_by_path(buf);

			if (inst_num) {
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnection.%d.PortMapping", IGD_WANDEVICE, wd_inst, wcd_inst, inst_num);
				compare_portmapping_nvram(buf, ext_port, in_port, proto, src_addr, in_client, desc, index);
			}

			if (!strcmp(dsl_proto, "pppoe") || !strcmp(dsl_proto, "pppoa")) {
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnection", IGD_WANDEVICE, wd_inst, wcd_inst);
				inst_num = get_inst_num_by_path(buf);

				if (inst_num) {
					snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnection.%d.PortMapping", IGD_WANDEVICE, wd_inst, wcd_inst, inst_num);
					compare_portmapping_nvram(buf, ext_port, in_port, proto, src_addr, in_client, desc, index);
				}
			}
		}
	}
}

void xdsl_reset_portmapping_count(int unit, int wd_inst)
{
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *dsl_proto = NULL;
#else 	/* DSL_ASUSWRT */
	char dsl_proto[8] = {0}, dsl_prefix[sizeof("wanXXXXXXXXXX_")] = {0};
#endif
	char buf[256]= {0};
	int wcd_inst = 0;//, inst_num = 0;
	int xdsl_internet_index[] = {ADSL_INTERNET_INDEX
#if defined(RTCONFIG_VDSL) || defined(TCSUPPORT_WAN_PTM)
								, VDSL_INTERNET_INDEX
#endif
								};
	int i = 0;

	for (i = 0; i < sizeof(xdsl_internet_index)/sizeof(int); i ++) {
#ifdef ASUSWRT
		snprintf(prefix, sizeof(prefix), "dsl%d", xdsl_internet_index[i]);
#else 	/* DSL_ASUSWRT */
#ifdef TCSUPPORT_WAN_ATM
		if (xdsl_internet_index[i] == WAN_ATM_INDEX)
			snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, WAN_ATM_INDEX);
#endif
#ifdef TCSUPPORT_WAN_PTM
		if (xdsl_internet_index[i] == WAN_PTM_INDEX)
			snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, WAN_PTM_INDEX);
#endif
#endif

		if ((wcd_inst = find_xdsl_wan_device(wd_inst, prefix))) {
#ifdef ASUSWRT
			dsl_proto = nvram_safe_get(strcat_r(prefix, "_proto", tmp));
#else 	/* DSL_ASUSWRT */
			snprintf(dsl_prefix, sizeof(dsl_prefix), "wan%d", xdsl_internet_index[i]);
			tcapi_get(WANDUCK_NODE, strcat_r(dsl_prefix, "_proto", tmp), dsl_proto);
#endif
			snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnection", IGD_WANDEVICE, wd_inst, wcd_inst);
			reset_portmapping_count(buf);			

			if (!strcmp(dsl_proto, "pppoe") || !strcmp(dsl_proto, "pppoa")) {
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnection", IGD_WANDEVICE, wd_inst, wcd_inst);
				reset_portmapping_count(buf);
			}
		}
	}
}

void add_del_xdsl_portmapping(int unit, int wd_inst, char *full_path, int nin, int flag)
{
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *dsl_proto = NULL;
#else 	/* DSL_ASUSWRT */
	char dsl_proto[8] = {0}, dsl_prefix[sizeof("wanXXXXXXXXXX_")] = {0};
#endif
	char buf[256]= {0};
	int wcd_inst = 0;
	int xdsl_internet_index[] = {ADSL_INTERNET_INDEX
#if defined(RTCONFIG_VDSL) || defined(TCSUPPORT_WAN_PTM)
								, VDSL_INTERNET_INDEX
#endif
								};
	int i = 0;

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	for (i = 0; i < sizeof(xdsl_internet_index)/sizeof(int); i ++) {
#ifdef ASUSWRT
		snprintf(prefix, sizeof(prefix), "dsl%d", xdsl_internet_index[i]);
#else 	/* DSL_ASUSWRT */
#ifdef TCSUPPORT_WAN_ATM
		if (xdsl_internet_index[i] == WAN_ATM_INDEX)
			snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, WAN_ATM_INDEX);
#endif
#ifdef TCSUPPORT_WAN_PTM
		if (xdsl_internet_index[i] == WAN_PTM_INDEX)
			snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, WAN_PTM_INDEX);
#endif
#endif
		if ((wcd_inst = find_xdsl_wan_device(wd_inst, prefix))) {
#ifdef ASUSWRT
			dsl_proto = nvram_safe_get(strcat_r(prefix, "_proto", tmp));
#else 	/* DSL_ASUSWRT */
			snprintf(dsl_prefix, sizeof(dsl_prefix), "wan%d", xdsl_internet_index[i]);
			tcapi_get(WANDUCK_NODE, strcat_r(dsl_prefix, "_proto", tmp), dsl_proto);
#endif

			/* for WANIPConnection */
			snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnection", IGD_WANDEVICE, wd_inst, wcd_inst);
			if (flag == PM_ADD)
				add_portmapping_entry(full_path, nin, buf);
			else if (flag == PM_DEL)
				del_portmapping_entry(full_path, nin, buf);

			if (!strcmp(dsl_proto, "pppoe") || !strcmp(dsl_proto, "pppoa")) {
				/* for WANPPPConnection */
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnection", IGD_WANDEVICE, wd_inst, wcd_inst);
				if (flag == PM_ADD)
					add_portmapping_entry(full_path, nin, buf);
				else if (flag == PM_DEL)
					del_portmapping_entry(full_path, nin, buf);
			}
		}
	}

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);
}

void set_xdsl_portmapping(int unit, int wd_inst, int nin, char *field, char *value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *dsl_proto = NULL;
#else 	/* DSL_ASUSWRT */
	char dsl_proto[8] = {0}, dsl_prefix[sizeof("wanXXXXXXXXXX_")] = {0};
#endif
	char buf[256]= {0};
	int wcd_inst = 0, inst_num = 0;
	int xdsl_internet_index[] = {ADSL_INTERNET_INDEX
#if defined(RTCONFIG_VDSL) || defined(TCSUPPORT_WAN_PTM)
								, VDSL_INTERNET_INDEX
#endif
								};
	int i = 0;

	for (i = 0; i < sizeof(xdsl_internet_index)/sizeof(int); i ++) {
#ifdef ASUSWRT
		snprintf(prefix, sizeof(prefix), "dsl%d", xdsl_internet_index[i]);
#else 	/* DSL_ASUSWRT */
#ifdef TCSUPPORT_WAN_ATM
		if (xdsl_internet_index[i] == WAN_ATM_INDEX)
			snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, WAN_ATM_INDEX);
#endif
#ifdef TCSUPPORT_WAN_PTM
		if (xdsl_internet_index[i] == WAN_PTM_INDEX)
			snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, WAN_PTM_INDEX);
#endif
#endif
		if ((wcd_inst = find_xdsl_wan_device(wd_inst, prefix))) {
#ifdef ASUSWRT
			dsl_proto = nvram_safe_get(strcat_r(prefix, "_proto", tmp));
#else 	/* DSL_ASUSWRT */
			snprintf(dsl_prefix, sizeof(dsl_prefix), "wan%d", xdsl_internet_index[i]);
			tcapi_get(WANDUCK_NODE, strcat_r(dsl_prefix, "_proto", tmp), dsl_proto);
#endif

			/* for WANIPConnection */
			snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnection", IGD_WANDEVICE, wd_inst, wcd_inst);
			inst_num = get_inst_num_by_path(buf);

			if (inst_num) {
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANIPConnection.%d.PortMapping.%d.%s", IGD_WANDEVICE, wd_inst, wcd_inst, inst_num, nin, field);
				tr_log(LOG_DEBUG, "%s - The path for set: %s", __FUNCTION__, buf);
				update_flag = 1;
				__set_parameter_value(buf, value);
				update_flag = 0;
			}			

			if (!strcmp(dsl_proto, "pppoe") || !strcmp(dsl_proto, "pppoa")) {
				/* for WANPPPConnection */
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnection", IGD_WANDEVICE, wd_inst, wcd_inst);
				inst_num = get_inst_num_by_path(buf);

				if (inst_num) {
					snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.%d.WANPPPConnection.%d.PortMapping.%d.%s", IGD_WANDEVICE, wd_inst, wcd_inst, inst_num, nin, field);
					tr_log(LOG_DEBUG, "%s - The path for set: %s", __FUNCTION__, buf);
					update_flag = 1;
					__set_parameter_value(buf, value);
					update_flag = 0;
				}
			}
		}
	}
}
#endif	//RTCONFIG_XDSL

int update_wan_device()
{
	char tmp[32] = {0}, prefix[sizeof("wanXXXXXXXXXX_")] = {0}, buf[256] = {0};
#ifdef ASUSWRT
	char *wan_proto = NULL;
	int wan_primary_unit = wan_primary_ifunit();
#else 	/* DSL_ASUSWRT */
	char wan_proto[16] = {0};
	int wan_primary_unit = tcapi_get_int(WANDUCK_NODE, "wan_primary");
#endif
	int inst_num = 0;

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	if (support_dualwan) {	/* support dual wan */
		int is_dualwan = 0;
		int wd_num = 1;	/* wan as default for ASUSWRT, dsl as default for DSL_ASUSWRT */
		char wd_num_str[4] = {0};

#ifdef ASUSWRT
		wd_num++;	/* for lan */
		if(!strstr(nvram_get("wans_dualwan"), "none"))
			is_dualwan = 1;
#else 	/* DSL_ASUSWRT */
		char wans_dualwan[32] = {0};

		tcapi_get(DUALWAN_NODE, "wans_dualwan", wans_dualwan);
		//wd_num++;	/* don't support for lan */
		wd_num++;	/* for wan */
		if(!strstr(wans_dualwan, "none"))
			is_dualwan = 1;		
#endif

#ifdef RTCONFIG_USB_MODEM
		wd_num++;	/* for usb */
#endif

		if (pri_wan_inst) {
#ifdef ASUSWRT
#ifdef RTCONFIG_XDSL
			if (wan_pri) {	/* update for dsl wan device */
				update_xdsl_wan_device(pri_wan_inst);
			}
			else 	/* update for lan or usb wan device */
#endif
			{
				/* check WANIPConnection and WANPPPConnection for primary wan */
				check_ip_ppp_connection("wan0_", pri_wan_inst);

				if ((is_dualwan && nvram_match("wans_mode", "lb")) || wan_primary_unit == WAN_UNIT_FIRST) {
					/* set static inform for ExternalIPAddress of WANIPConnection/WANPPPConnection */
					snprintf(prefix, sizeof(prefix), "wan0_");
					wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
#else 	/* DSL_ASUSWRT */
#ifdef RTCONFIG_XDSL
			if (dsl_pri) {	/* update for dsl wan device */
				update_xdsl_wan_device(pri_wan_inst);
			}
			else 	/* update for wan, usb device */
#endif	
			{
				char wans_mode[8] = {0};
				int wan_index = 0;

				if (dsl_pri) {
#ifdef TCSUPPORT_WAN_ATM
					wan_index = WAN_ATM_INDEX;
#endif
#ifdef TCSUPPORT_WAN_PTM
					wan_index = WAN_PTM_INDEX;
#endif
				}
#ifdef TCSUPPORT_WAN_ETHER			
				else if (wan_pri) {
					wan_index = WAN_ETHER_INDEX;
				}
#endif
#ifdef RTCONFIG_USB_MODEM
				else if (usb_pri) {
					wan_index = WAN_USB_INDEX;
				}
#endif

				tcapi_get(DUALWAN_NODE, "wans_mode", wans_mode);

				/* check WANIPConnection and WANPPPConnection for primary wan */
				snprintf(prefix, sizeof(prefix), "wan%d_", wan_index);
				check_ip_ppp_connection(prefix, pri_wan_inst);

				if ((is_dualwan && !strcmp(wans_mode, "lb")) || wan_primary_unit == wan_index) {	
					/* set static inform for ExternalIPAddress of WANIPConnection/WANPPPConnection */
					tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);	
#endif

					if (!strcmp(wan_proto, "dhcp") || !strcmp(wan_proto, "static")) {
						snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnection", IGD_WANDEVICE, pri_wan_inst);
						inst_num = get_inst_num_by_path(buf);

						if (inst_num) {
							snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnection.%d.ExternalIPAddress", IGD_WANDEVICE, pri_wan_inst, inst_num);
							add_inform_parameter(buf, 0);
						}
					}
					else
					{
						snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection", IGD_WANDEVICE, pri_wan_inst);
						inst_num = get_inst_num_by_path(buf);

						if (inst_num) {
							snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection.%d.ExternalIPAddress", IGD_WANDEVICE, pri_wan_inst, inst_num);
							add_inform_parameter(buf, 0);
						}
					}
				}
			}
		}

		if (sec_wan_inst) {
#ifdef ASUSWRT
#ifdef RTCONFIG_XDSL
			if (wan_sec) {	/* update for dsl wan device */
				update_xdsl_wan_device(sec_wan_inst);
			}
			else 	/* update for lan or usb wan device */
#endif
			{
				/* check WANIPConnection and WANPPPConnection for secondary wan */
				check_ip_ppp_connection("wan1_", sec_wan_inst);
				if ((is_dualwan && nvram_match("wans_mode", "lb")) || wan_primary_unit == WAN_UNIT_SECOND) {
					/* set static inform for ExternalIPAddress of WANIPConnection/WANPPPConnection */
					snprintf(prefix, sizeof(prefix), "wan1_");
					wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
#else 	/* DSL_ASUSWRT */
#ifdef RTCONFIG_XDSL
			if (dsl_sec) {	/* update for dsl wan device */
				update_xdsl_wan_device(sec_wan_inst);
			}
			else 	/* update for wan, usb device */
#endif	
			{
				char wans_mode[8] = {0};
				int wan_index = 0;

				if (dsl_sec) {
#ifdef TCSUPPORT_WAN_ATM
					wan_index = WAN_ATM_INDEX;
#endif
#ifdef TCSUPPORT_WAN_PTM
					wan_index = WAN_PTM_INDEX;
#endif
				}
#ifdef TCSUPPORT_WAN_ETHER			
				else if (wan_sec) {
					wan_index = WAN_ETHER_INDEX;
				}
#endif
#ifdef RTCONFIG_USB_MODEM
				else if (usb_sec) {
					wan_index = WAN_USB_INDEX;
				}
#endif

				tcapi_get(DUALWAN_NODE, "wans_mode", wans_mode);

				/* check WANIPConnection and WANPPPConnection for secondary wan */
				snprintf(prefix, sizeof(prefix), "wan%d_", wan_index);
				check_ip_ppp_connection(prefix, sec_wan_inst);

				if ((is_dualwan && !strcmp(wans_mode, "lb")) || wan_primary_unit == wan_index) {
					/* set static inform for ExternalIPAddress of WANIPConnection/WANPPPConnection */
					tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);			
#endif

					if (!strcmp(wan_proto, "dhcp") || !strcmp(wan_proto, "static")) {
						snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnection", IGD_WANDEVICE, sec_wan_inst);
						inst_num = get_inst_num_by_path(buf);

						if (inst_num) {
							snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnection.%d.ExternalIPAddress", IGD_WANDEVICE, sec_wan_inst, inst_num);
							add_inform_parameter(buf, 0);
						}
					}
					else
					{
						snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection", IGD_WANDEVICE, sec_wan_inst);
						inst_num = get_inst_num_by_path(buf);

						if (inst_num) {
							snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection.%d.ExternalIPAddress", IGD_WANDEVICE, sec_wan_inst, inst_num);
							add_inform_parameter(buf, 0);
						}
					}
				}
			}
		}

		/* set InternetGatewayDevice.WANDeviceNumberOfEntries as wan or dsl + lan + usb */
		snprintf(wd_num_str, sizeof(wd_num_str), "%d", wd_num);
		__set_parameter_value(IGD_WANDEVICE_NUM, wd_num_str);	
	}
	else /* doesn't support dual wan */
	{
		node_t node;

#ifdef DSL_ASUSWRT
#ifndef TCSUPPORT_WAN_ETHER
		/* check and delete wan device for usb */
		snprintf(buf, sizeof(buf), "%s.%d.", IGD_WANDEVICE, IGD_WANDEVICE_WAN_INST);
		if(lib_resolve_node(buf, &node) == 0)
			delete_object(buf, strlen(buf));
#endif
#endif

		/* check and delete wan device for lan */
		snprintf(buf, sizeof(buf), "%s.%d.", IGD_WANDEVICE, IGD_WANDEVICE_LAN_INST);
		if (lib_resolve_node(buf, &node) == 0)
			delete_object(buf, strlen(buf));

		/* check and delete wan device for usb */
		snprintf(buf, sizeof(buf), "%s.%d.", IGD_WANDEVICE, IGD_WANDEVICE_USB_INST);
		if(lib_resolve_node(buf, &node) == 0)
			delete_object(buf, strlen(buf));

#ifdef RTCONFIG_XDSL
		update_xdsl_wan_device(pri_wan_inst);
#else
		/* check WANIPConnection andWANPPPConnection for primary wan */
		check_ip_ppp_connection("wan0_", IGD_WANDEVICE_WAN_INST);

		/* set InternetGatewayDevice.WANDeviceNumberOfEntries as 1*/
		__set_parameter_value(IGD_WANDEVICE_NUM, "1");	

		/* set static inform for ExternalIPAddress of WANIPConnection/WANPPPConnection */
		snprintf(prefix, sizeof(prefix), "wan%d_", wan_primary_unit);
		wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
		if (!strcmp(wan_proto, "dhcp") || !strcmp(wan_proto, "static")) {
			snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnection", IGD_WANDEVICE, IGD_WANDEVICE_WAN_INST);
			inst_num = get_inst_num_by_path(buf);

			if (inst_num)
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnection.%d.ExternalIPAddress", IGD_WANDEVICE, IGD_WANDEVICE_WAN_INST, inst_num);
		}
		else
		{
			snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection", IGD_WANDEVICE, IGD_WANDEVICE_WAN_INST);
			inst_num = get_inst_num_by_path(buf);

			if (inst_num)
				snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection.%d.ExternalIPAddress", IGD_WANDEVICE, IGD_WANDEVICE_WAN_INST, inst_num);
		}
		add_inform_parameter(buf, 0);
#endif

		/* set InternetGatewayDevice.WANDeviceNumberOfEntries only one wan */
		__set_parameter_value(IGD_WANDEVICE_NUM, "1");			
	}

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

static int add_eth_wanipconnection(node_t node, char *arg, int nin)
{
	char tmp[32] = {0}, prefix[sizeof("wanXXXXXXXXXX_")] = {0}, buf[256] = {0};
	char *path = lib_node2path(node);
	int unit = 0, wd_inst = 0;

	unit = get_wan_unit_by_path(path);
#ifdef ASUSWRT
	wd_inst = get_wd_inst_by_wan_unit(unit);
#else 	/* DSL_ASUSWRT */
	wd_inst = getWANDevInstNum(path);
	memset(prefix, 0, sizeof(prefix));	
	snprintf(prefix, sizeof(prefix), "%s", eth_wanip_prefix_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;
#endif

    /* Set wanX_proto as dhcp */
#ifdef ASUSWRT
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
    char *wan_proto = NULL;
    wan_proto=nvram_safe_get(strcat_r(prefix, "proto", tmp));
   //if (strcmp(wan_proto, "static"))
   if (strcmp(wan_proto, "pptp") &&strcmp(wan_proto, "l2tp")&&strcmp(wan_proto, "pppoe")&&strcmp(wan_proto, "static"))
    {
        nvram_set(strcat_r(prefix, "proto", tmp), "dhcp");
    }
#else 	/* DSL_ASUSWRT */
    char wan_proto[16] = {0};
    tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);
    if (strcmp(wan_proto, "pptp") &&strcmp(wan_proto, "l2tp")&&strcmp(wan_proto, "pppoe")&&strcmp(wan_proto, "static"))
    //if (strcmp(wan_proto, "static"))
    {
        tcapi_set(prefix, "ISP", "0");	/* dhcp */
    }
#endif

	/* Update WANIPConnectionNumberOfEntries and some infor */
	snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnectionNumberOfEntries", IGD_WANDEVICE, wd_inst);
	__set_parameter_value(buf, "1");

	return 0;
}

static int del_eth_wanipconnection(node_t node, char *arg, int nin)
{
	char tmp[32] = {0}, prefix[sizeof("wanXXXXXXXXXX_")] = {0}, buf[256] = {0};
	char *path = lib_node2path(node);
	int unit = 0, wd_inst = 0, inst_num = 0;

	unit = get_wan_unit_by_path(path);
#ifdef ASUSWRT
	wd_inst = get_wd_inst_by_wan_unit(unit);
#else 	/* DSL_ASUSWRT */
	wd_inst = getWANDevInstNum(path);
	memset(prefix, 0, sizeof(prefix));	
	snprintf(prefix, sizeof(prefix), "%s", eth_wanip_prefix_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;
#endif

	/* Set wanX_proto as dhcp */
#ifdef ASUSWRT
    //snprintf(prefix, sizeof(prefix), "wan%d_", unit);
    //nvram_set(strcat_r(prefix, "proto", tmp), "dhcp");
    snprintf(prefix, sizeof(prefix), "wan%d_", unit);
    char *wan_proto = NULL;
     wan_proto=nvram_safe_get(strcat_r(prefix, "proto", tmp));
     if (strcmp(wan_proto, "static"))
     {
          nvram_set(strcat_r(prefix, "proto", tmp), "dhcp");
      }
#else 	/* DSL_ASUSWRT */
    //tcapi_set(prefix, "ISP", "0");	/* dhcp */
    char wan_proto[16] = {0};
    tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);
    if (strcmp(wan_proto, "static"))
    {
        tcapi_set(prefix, "ISP", "0");	/* dhcp */
    }
#endif

	/* Delete WANPPPConnection */
	snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection", IGD_WANDEVICE, wd_inst);
	inst_num = get_inst_num_by_path(buf);
	if (inst_num) {
		snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnection.%d.", IGD_WANDEVICE, wd_inst, inst_num);
		update_flag = 1;	/* Avoid to execute the corresponding delete function */
		tr_log(LOG_DEBUG, "%s - %s %d", __FUNCTION__, buf, __do__(buf, strlen(buf)));
		update_flag = 0;	/* Return to normal process */

		/* Update WANPPPConnectionNumberOfEntries and some info */
		snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnectionNumberOfEntries", IGD_WANDEVICE, wd_inst);
		__set_parameter_value(buf, "0");
	}

	/* Update WANIPConnectionNumberOfEntries and some info */
	snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANIPConnectionNumberOfEntries", IGD_WANDEVICE, wd_inst);
	__set_parameter_value(buf, "0");

	return 0;
}

static int add_eth_wanpppconnection(node_t node, char *arg, int nin)
{
	char tmp[32] = {0}, prefix[sizeof("wanXXXXXXXXXX_")] = {0}, buf[256] = {0};
	char *path = lib_node2path(node);
	int unit = 0, wd_inst = 0;

	unit = get_wan_unit_by_path(path);
#ifdef ASUSWRT
	wd_inst = get_wd_inst_by_wan_unit(unit);	
#else 	/* DSL_ASUSWRT */
	wd_inst = getWANDevInstNum(path);
	memset(prefix, 0, sizeof(prefix));	
	snprintf(prefix, sizeof(prefix), "%s", eth_wanip_prefix_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;
#endif

	/* Set wanX_proto as pppoe */
#ifdef ASUSWRT
    //snprintf(prefix, sizeof(prefix), "wan%d_", unit);
    //nvram_set(strcat_r(prefix, "proto", tmp), "pppoe");
    snprintf(prefix, sizeof(prefix), "wan%d_", unit);
    char *wan_proto = NULL;
     wan_proto=nvram_safe_get(strcat_r(prefix, "proto", tmp));
     if (strcmp(wan_proto, "pptp") &&strcmp(wan_proto, "l2tp"))
     {
         nvram_set(strcat_r(prefix, "proto", tmp), "pppoe");
     }
#else 	/* DSL_ASUSWRT */
    //tcapi_set(prefix, "ISP", "2");	/* pppoe */
    char wan_proto[16] = {0};
    tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);
    if (strcmp(wan_proto, "pptp") &&strcmp(wan_proto, "l2tp"))
    {
        tcapi_set(prefix, "ISP", "2");	/* pppoe */
     }
#endif

	/* Update WANPPPConnectionNumberOfEntries and some infor */
	snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnectionNumberOfEntries", IGD_WANDEVICE, wd_inst);
	__set_parameter_value(buf, "1");

	return 0;
}

static int del_eth_wanpppconnection(node_t node, char *arg, int nin)
{
	char tmp[32] = {0}, prefix[sizeof("wanXXXXXXXXXX_")] = {0}, buf[256] = {0};
	char *path = lib_node2path(node);
	int unit = 0, wd_inst = 0;

	unit = get_wan_unit_by_path(path);
#ifdef ASUSWRT
	wd_inst = get_wd_inst_by_wan_unit(unit);
#else 	/* DSL_ASUSWRT */
	wd_inst = getWANDevInstNum(path);
	memset(prefix, 0, sizeof(prefix));	
	snprintf(prefix, sizeof(prefix), "%s", eth_wanip_prefix_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;
#endif

	/* Set wanX_proto as dhcp */
#ifdef ASUSWRT
    //snprintf(prefix, sizeof(prefix), "wan%d_", unit);
    //nvram_set(strcat_r(prefix, "proto", tmp), "dhcp");
    snprintf(prefix, sizeof(prefix), "wan%d_", unit);
    char *wan_proto = NULL;
     wan_proto=nvram_safe_get(strcat_r(prefix, "proto", tmp));
     if (strcmp(wan_proto, "static"))
     {
          nvram_set(strcat_r(prefix, "proto", tmp), "dhcp");
      }
#else 	/* DSL_ASUSWRT */
    //tcapi_set(prefix, "ISP", "0");	/* dhcp */
    char wan_proto[16] = {0};
    tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);
    if (strcmp(wan_proto, "static"))
    {
        tcapi_set(prefix, "ISP", "0");	/* dhcp */
    }
#endif

	/* Update WANPPPConnectionNumberOfEntries and some info */
	snprintf(buf, sizeof(buf), "%s.%d.WANConnectionDevice.1.WANPPPConnectionNumberOfEntries", IGD_WANDEVICE, wd_inst);
	__set_parameter_value(buf, "0");

	return 0;
}

static int set_eth_natenabled(node_t node, char *arg, char *value)
{
#ifdef ASUSWRT
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);
#endif
	int res = 0;
	char *value_conv = NULL;

	value_conv = (!war_strcasecmp(value, "1") || !war_strcasecmp(value, "true")) ? "1" : "0";

#ifdef ASUSWRT
	memset(prefix, 0, sizeof(prefix));
	snprintf(prefix, sizeof(prefix), "%s", eth_wanip_prefix_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;

#ifdef RTCONFIG_XDSL
	if (check_dsl_wan_device_by_path(path))
		res = set_nvram("dslx_nat", value_conv);
	else
#endif
	res = set_nvram(strcat_r(prefix, "nat_x", tmp), value_conv);
#else 	/* DSL_ASUSWRT */
	res = set_nvram(WAN_COMMON_NODE, "NATENABLE", !strcmp(value_conv, "1") ? "Enable" : "Disabled");
#endif

	return res;
}

static int get_eth_natenabled(node_t node, char *arg, char **value)
{
	char buf[16] = {0};
#ifdef ASUSWRT
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));	
	snprintf(prefix, sizeof(prefix), "%s", eth_wanip_prefix_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;

	snprintf(buf, sizeof(buf), "%s", nvram_match(strcat_r(prefix, "nat_x", tmp), "1") ? "true" : "false");
#else 	/* DSL_ASUSWRT */
	snprintf(buf, sizeof(buf), "%s", tcapi_match(WAN_COMMON_NODE, "NATENABLE", "Enable") ? "true" : "false");
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_eth_addressingtype(node_t node, char *arg, char *value)
{
	char prefix[16] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *wan_proto = NULL;
#endif
	int res = 0;
	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));	
	snprintf(prefix, sizeof(prefix), "%s", eth_wanip_prefix_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp") 
#ifdef RTCONFIG_XDSL
		|| !strcmp(wan_proto, "pppoa")
#endif
		) {
#ifdef RTCONFIG_XDSL
		if (check_dsl_wan_device_by_path(path))
			res = set_nvram("dslx_DHCPClient", !strcasecmp(value, "dhcp") ? "1" : "0");
		else
#endif
		res = set_nvram(strcat_r(prefix, "dhcpenable_x", tmp), !strcasecmp(value, "dhcp") ? "1" : "0");
	}
	else
	{
		if (!strcasecmp(value, "DHCP")) {
#ifdef RTCONFIG_XDSL
			if (check_dsl_wan_device_by_path(path)) {
#ifdef RTCONFIG_DSL
				if (nvram_match("dslx_transmode", ADSL_TRANS_MODE))
					res = set_nvram("dsl0_proto", "mer");
#endif
#ifdef RTCONFIG_VDSL
				if (nvram_match("dslx_transmode", VDSL_TRANS_MODE))
					res = set_nvram("dsl8_proto", "dhcp");
#endif				
			}
			else
#endif			
			res = set_nvram(strcat_r(prefix, "proto", tmp), "dhcp");
		}
		else if(!strcasecmp(value, "Static")) {
#ifdef RTCONFIG_XDSL
			if (check_dsl_wan_device_by_path(path)) {
#ifdef RTCONFIG_DSL
				if (nvram_match("dslx_transmode", ADSL_TRANS_MODE))
					res = set_nvram("dsl0_proto", "ipoa");
#endif
#ifdef RTCONFIG_VDSL
				if (nvram_match("dslx_transmode", VDSL_TRANS_MODE))
					res = set_nvram("dsl8_proto", "static");
#endif				
			}
			else
#endif			
			res = set_nvram(strcat_r(prefix, "proto", tmp), "static");
		}
	}

#else 	/* DSL_ASUSWRT */

	res = set_nvram(convert_prefix_to_pvc(prefix, tmp), "ISP", !strcasecmp(value, "dhcp") ? "0" : "1");
#endif

	return res;
}

static int set_eth_externalipaddress(node_t node, char *arg, char *value)
{
	char prefix[16] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *wan_proto = NULL, *wan_dhcpenable = NULL;
#else 	/* DSL_ASUSWRT */
	char pvc[32] = {0};
#endif
	int res = 0;
	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));	
	snprintf(prefix, sizeof(prefix), "%s", eth_wanip_prefix_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	wan_dhcpenable = nvram_safe_get(strcat_r(prefix, "dhcpenable_x", tmp));

	if(strstr(path, "WANIPConnection") != NULL) {	/* WANIPConnection */
		if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp")
#ifdef RTCONFIG_XDSL
			|| !strcmp(wan_proto, "pppoa")
#endif			
			) {
			if(!strcmp(wan_dhcpenable, "1")
#ifdef RTCONFIG_XDSL
				|| !strcmp(wan_dhcpenable, "2")
#endif
			)	/* can't be modified */
				return -1;
			else
			{
#ifdef RTCONFIG_XDSL
				if (check_dsl_wan_device_by_path(path))
					res = set_nvram("dslx_ipaddr", value);
				else
#endif
				res = set_nvram(strcat_r(prefix, "ipaddr_x", tmp), value);
			}
		}
		else
		{
			if(!strcmp(wan_proto, "dhcp")
#ifdef RTCONFIG_XDSL
				|| (!strcmp(wan_proto, "mer") && !strcmp(wan_dhcpenable, "1"))
#endif
			)	/* can't be modified */
				return -1;
			else
			{
#ifdef RTCONFIG_XDSL
				if (check_dsl_wan_device_by_path(path))
					res = set_nvram("dslx_ipaddr", value);
				else
#endif				
				res = set_nvram(strcat_r(prefix, "ipaddr_x", tmp), value);
			}
		}
	}
	else	/* WANPPPConnection */		/* can't be modified */
		return -1;

#else 	/* DSL_ASUSWRT */
	if(strstr(path, "WANIPConnection") != NULL) {	/* WANIPConnection */
		snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
		if (tcapi_match(pvc, "ISP", "1")) 	/* static */
			res = set_nvram(pvc, "IPADDR", value);
		else
			return -1;	/* can't be modified */
	}	
	else	/* WANPPPConnection */		/* can't be modified */
		return -1;
#endif

	return res;
}

static int get_eth_externalipaddress(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[16] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *wan_proto = NULL, *wan_dhcpenable = NULL;
#else 	/* DSL_ASUSWRT */
	char ipaddr[32] = {0}, pvc[32] = {0};
#endif
	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));	
	snprintf(prefix, sizeof(prefix), "%s", eth_wanip_prefix_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	wan_dhcpenable = nvram_safe_get(strcat_r(prefix, "dhcpenable_x", tmp));

	if(strstr(path, "WANIPConnection") != NULL) {	/* WANIPConnection */
		if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp") 
#ifdef RTCONFIG_XDSL
			|| !strcmp(wan_proto, "pppoa")
#endif			
			) {
			snprintf(buf, sizeof(buf), "%s", !strcmp(wan_dhcpenable, "1") 
#ifdef RTCONFIG_XDSL
												|| !strcmp(wan_dhcpenable, "2")
#endif
							? nvram_safe_get(strcat_r(prefix, "xipaddr", tmp)) : 
							nvram_safe_get(strcat_r(prefix, "ipaddr_x", tmp)));
		}
		else
			snprintf(buf, sizeof(buf), "%s", !strcmp(wan_proto, "dhcp") 
#ifdef RTCONFIG_XDSL
											|| (!strcmp(wan_proto, "mer") && !strcmp(wan_dhcpenable, "1"))
#endif				
							? nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)) : 
							nvram_safe_get(strcat_r(prefix, "ipaddr_x", tmp)));
	}
	else	/* WANPPPConnection */
		snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)));

#else 	/* DSL_ASUSWRT */
	snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
	if (tcapi_match(pvc, "ISP", "0"))	/* dhcp */
		snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "ipaddr", tmp), ipaddr));
	else if (tcapi_match(pvc, "ISP", "1")) 	/* static */
		snprintf(buf, sizeof(buf), "%s", tcapi_get_string(pvc, "IPADDR", ipaddr));
	else if (tcapi_match(pvc, "ISP", "2")) {
		if (tcapi_match(pvc, "PPPGETIP", "Static"))
			snprintf(buf, sizeof(buf), "%s", tcapi_get_string(pvc, "IPADDR", ipaddr));
		else
			snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "ipaddr", tmp), ipaddr));
	}
	else
		return -1;
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_eth_subnetmask(node_t node, char *arg, char *value)
{
	char prefix[16] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *wan_proto = NULL, *wan_dhcpenable = NULL;
#else 	/* DSL_ASUSWRT */
	char pvc[32] = {0};
#endif
	int res = 0;
	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));	
	snprintf(prefix, sizeof(prefix), "%s", eth_wanip_prefix_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	wan_dhcpenable = nvram_safe_get(strcat_r(prefix, "dhcpenable_x", tmp));

	if(strstr(path, "WANIPConnection") != NULL) {	/* WANIPConnection */
		if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp")
#ifdef RTCONFIG_XDSL
			|| !strcmp(wan_proto, "pppoa")
#endif			
			) {
			if(!strcmp(wan_dhcpenable, "1")
#ifdef RTCONFIG_XDSL
				|| !strcmp(wan_dhcpenable, "2")
#endif
			)	/* can't be modified */
				return -1;
			else
			{
#ifdef RTCONFIG_XDSL
				if (check_dsl_wan_device_by_path(path))
					res = set_nvram("dslx_netmask", value);
				else
#endif
				res = set_nvram(strcat_r(prefix, "netmask_x", tmp), value);
			}
		}
		else
		{
			if(!strcmp(wan_proto, "dhcp")
#ifdef RTCONFIG_XDSL
				|| (!strcmp(wan_proto, "mer") && !strcmp(wan_dhcpenable, "1"))
#endif
			)	/* can't be modified */
				return -1;
			else
			{
#ifdef RTCONFIG_XDSL
				if (check_dsl_wan_device_by_path(path))
					res = set_nvram("dslx_netmask", value);
				else
#endif
				res = set_nvram(strcat_r(prefix, "netmask_x", tmp), value);
			}
		}
	}
	else	/* WANPPPConnection */		/* can't be modified */
		return -1;

#else 	/* DSL_ASUSWRT */
	if(strstr(path, "WANIPConnection") != NULL) {	/* WANIPConnection */
		snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
		if (tcapi_match(pvc, "ISP", "1")) 	/* static */
			res = set_nvram(pvc, "NETMASK", value);
		else
			return -1;	/* can't be modified */
	}	
	else	/* WANPPPConnection */		/* can't be modified */
		return -1;
#endif

	return res;
}

static int get_eth_subnetmask(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[16] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *wan_proto = NULL, *wan_dhcpenable = NULL;
#else 	/* DSL_ASUSWRT */
	char netmask[32] = {0}, pvc[32] = {0};
#endif
	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));	
	snprintf(prefix, sizeof(prefix), "%s", eth_wanip_prefix_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	wan_dhcpenable = nvram_safe_get(strcat_r(prefix, "dhcpenable_x", tmp));
	
	if(strstr(path, "WANIPConnection") != NULL) {	/* WANIPConnection */
		if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp") 
#ifdef RTCONFIG_XDSL
			|| !strcmp(wan_proto, "pppoa")
#endif		
			) {
			snprintf(buf, sizeof(buf), "%s", !strcmp(wan_dhcpenable, "1") 
#ifdef RTCONFIG_XDSL
											|| !strcmp(wan_dhcpenable, "2")
#endif	
							? nvram_safe_get(strcat_r(prefix, "xnetmask", tmp)) : 
							nvram_safe_get(strcat_r(prefix, "netmask_x", tmp)));
		}
		else
			snprintf(buf, sizeof(buf), "%s", !strcmp(wan_proto, "dhcp") 
#ifdef RTCONFIG_XDSL
											|| (!strcmp(wan_proto, "mer") && !strcmp(wan_dhcpenable, "1"))
#endif					
							? nvram_safe_get(strcat_r(prefix, "netmask", tmp)) : 
							nvram_safe_get(strcat_r(prefix, "netmask_x", tmp)));
	}
	else	/* WANPPPConnection */
		snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "netmask", tmp)));

#else 	/* DSL_ASUSWRT */
	snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
	if (tcapi_match(pvc, "ISP", "0"))	/* dhcp */
		snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "netmask", tmp), netmask));
	else if (tcapi_match(pvc, "ISP", "1")) 	/* static */
		snprintf(buf, sizeof(buf), "%s", tcapi_get_string(pvc, "NETMASK", netmask));
	else if (tcapi_match(pvc, "ISP", "2")) {
		if (tcapi_match(pvc, "PPPGETIP", "Static"))
			snprintf(buf, sizeof(buf), "%s", tcapi_get_string(pvc, "NETMASK", netmask));
		else
			snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "netmask", tmp), netmask));
	}
	else
		return -1;
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_eth_defaultgateway(node_t node, char *arg, char *value)
{
	char prefix[16] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *wan_proto = NULL, *wan_dhcpenable = NULL;
#else 	/* DSL_ASUSWRT */
	char pvc[32] = {0};
#endif
	int res = 0;
	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));	
	snprintf(prefix, sizeof(prefix), "%s", eth_wanip_prefix_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	wan_dhcpenable = nvram_safe_get(strcat_r(prefix, "dhcpenable_x", tmp));

	if(strstr(path, "WANIPConnection") != NULL) {	/* WANIPConnection */
		if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp")
#ifdef RTCONFIG_DSL
			|| !strcmp(wan_proto, "pppoa")
#endif				
			) {
			if(!strcmp(wan_dhcpenable, "1")
#ifdef RTCONFIG_DSL
				|| !strcmp(wan_dhcpenable, "2")
#endif	
			)	/* can't be modified */
				return -1;
			else
			{
#ifdef RTCONFIG_DSL
				if (check_dsl_wan_device_by_path(path))
					res = set_nvram("dslx_gateway", value);
				else
#endif
				res = set_nvram(strcat_r(prefix, "gateway_x", tmp), value);
			}
		}
		else
		{
			if(!strcmp(wan_proto, "dhcp")
#ifdef RTCONFIG_DSL
				|| (!strcmp(wan_proto, "mer") && !strcmp(wan_dhcpenable, "1"))
#endif	
			)	/* can't be modified */
				return -1;
			else
			{
#ifdef RTCONFIG_DSL
				if (check_dsl_wan_device_by_path(path))
					res = set_nvram("dslx_gateway", value);
				else
#endif
				res = set_nvram(strcat_r(prefix, "gateway_x", tmp), value);
			}
		}
	}
	else	/* WANPPPConnection */		/* can't be modified */
		return -1;

#else 	/* DSL_ASUSWRT */
	if(strstr(path, "WANIPConnection") != NULL) {	/* WANIPConnection */
		snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
		if (tcapi_match(pvc, "ISP", "1")) 	/* static */
			res = set_nvram(pvc, "GATEWAY", value);
		else
			return -1;	/* can't be modified */
	}	
	else	/* WANPPPConnection */		/* can't be modified */
		return -1;
#endif

	return res;
}

static int get_eth_defaultgateway(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[16] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *wan_proto = NULL, *wan_dhcpenable = NULL;
#else 	/* DSL_ASUSWRT */
	char gateway[32] = {0}, pvc[32] = {0};
#endif
	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));	
	snprintf(prefix, sizeof(prefix), "%s", eth_wanip_prefix_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	wan_dhcpenable = nvram_safe_get(strcat_r(prefix, "dhcpenable_x", tmp));
	
	if(strstr(path, "WANIPConnection") != NULL) {	/* WANIPConnection */
		if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp") 
#ifdef RTCONFIG_DSL
			|| !strcmp(wan_proto, "pppoa")
#endif		
			) {
			snprintf(buf, sizeof(buf), "%s", !strcmp(wan_dhcpenable, "1") 
#ifdef RTCONFIG_DSL
											|| !strcmp(wan_dhcpenable, "2")
#endif						
							? nvram_safe_get(strcat_r(prefix, "xgateway", tmp)) : 
							nvram_safe_get(strcat_r(prefix, "gateway_x", tmp)));
		}
		else
			snprintf(buf, sizeof(buf), "%s", !strcmp(wan_proto, "dhcp") 
#ifdef RTCONFIG_DSL
											|| (!strcmp(wan_proto, "mer") && !strcmp(wan_dhcpenable, "1"))
#endif		
							? nvram_safe_get(strcat_r(prefix, "gateway", tmp)) : 
							nvram_safe_get(strcat_r(prefix, "gateway_x", tmp)));
	}
	else	/* WANPPPConnection */
		snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "gateway", tmp)));

#else 	/* DSL_ASUSWRT */
	snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
	if (tcapi_match(pvc, "ISP", "0"))	/* dhcp */
		snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "gateway", tmp), gateway));
	else if (tcapi_match(pvc, "ISP", "1")) 	/* static */
		snprintf(buf, sizeof(buf), "%s", tcapi_get_string(pvc, "GATEWAY", gateway));
	else if (tcapi_match(pvc, "ISP", "2")) {
		if (tcapi_match(pvc, "PPPGETIP", "Static"))
			snprintf(buf, sizeof(buf), "%s", tcapi_get_string(pvc, "GATEWAY", gateway));
		else
			snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "gateway", tmp), gateway));
	}
	else
		return -1;
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_eth_dnsoverrideallowed(node_t node, char *arg, char *value)
{
	char prefix[16] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *wan_proto = NULL;
#else 	/* DSL_ASUSWRT */
	char pvc[32] = {0};
#endif
	char *value_conv = NULL;
	int res = 0;

	value_conv = (!war_strcasecmp(value, "1") || !war_strcasecmp(value, "true")) ? "0" : "1";

	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));	
	snprintf(prefix, sizeof(prefix), "%s", eth_wanip_prefix_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	if(strstr(path, "WANIPConnection") != NULL) {	/* WANIPConnection */
		if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp")
#ifdef RTCONFIG_DSL
			|| !strcmp(wan_proto, "pppoa")
#endif		
			)	
		{
#ifdef RTCONFIG_DSL
			if (check_dsl_wan_device_by_path(path))
				res = set_nvram("dslx_dnsenable", value_conv);
			else
#endif			
			res = set_nvram(strcat_r(prefix, "dnsenable_x", tmp), value_conv);
		}
		else
		{
			if(!strcmp(wan_proto, "dhcp")
#ifdef RTCONFIG_DSL
				|| !strcmp(wan_proto, "mer")
#endif
				)
			{
#ifdef RTCONFIG_DSL
				if (check_dsl_wan_device_by_path(path))
					res = set_nvram("dslx_dnsenable", value_conv);
				else
#endif					
				res = set_nvram(strcat_r(prefix, "dnsenable_x", tmp), value_conv);
			}
			else		/* can't be modified */
				return -1;
		}
	}
	else	/* WANPPPConnection */	/* can't be modified */
		return -1;

#else 	/* DSL_ASUSWRT */
	if(strstr(path, "WANIPConnection") != NULL) {	/* WANIPConnection */
		snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
		if (tcapi_match(pvc, "ISP", "0"))	/* dhcp */
			res = set_nvram(pvc, "DNS_type", !strcmp(value_conv, "1") ? "0" : "1");
		if (tcapi_match(pvc, "ISP", "1")) { 	/* static */
			if (!strcmp(value_conv, "0"))
				return -1;
		}
		else
			return -1;	/* can't be modified */
	}	
	else	/* WANPPPConnection */		/* can't be modified */
		return -1;
#endif

	return res;
}

static int get_eth_dnsoverrideallowed(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[16] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *wan_proto = NULL, *wan_dnsenable = NULL;
#else 	/* DSL_ASUSWRT */
	char pvc[32] = {0};
#endif
	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));	
	snprintf(prefix, sizeof(prefix), "%s", eth_wanip_prefix_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	wan_dnsenable = nvram_safe_get(strcat_r(prefix, "dnsenable_x", tmp));
	
	if(strstr(path, "WANIPConnection") != NULL) {	/* WANIPConnection */
		if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp") 
#ifdef RTCONFIG_DSL
			|| !strcmp(wan_proto, "pppoa")
#endif		
			)
			snprintf(buf, sizeof(buf), "%s", !strcmp(wan_dnsenable, "1") ? "false": "true");
		else
		{
			if(!strcmp(wan_proto, "dhcp")
#ifdef RTCONFIG_DSL
				|| !strcmp(wan_proto, "mer")
#endif	
				)
				snprintf(buf, sizeof(buf), "%s", !strcmp(wan_dnsenable, "1") ? "false": "true");
			else
				snprintf(buf, sizeof(buf), "true");
		}
	}
	else	/* WANPPPConnection */
		return -1;

#else 	/* DSL_ASUSWRT */
	snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
	if(strstr(path, "WANIPConnection") != NULL) {	/* WANIPConnection */
		if (tcapi_match(pvc, "ISP", "0"))	/* dhcp */
			snprintf(buf, sizeof(buf), "%s", tcapi_match(pvc, "DNS_type", "1") ? "false" : "true");
		else if (tcapi_match(pvc, "ISP", "1")) 	/* static */
			snprintf(buf, sizeof(buf), "false");
		else
			return -1;
	}
	else	/* WANPPPConnection */
		return -1;
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_wlan_bssid(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char hwaddr[32] = {0}, buf[32] = {0};
	unsigned char ea[6] = {0};
	int subunit = 0;
#endif

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	return get_nvram(strcat_r(prefix, "hwaddr", tmp), value);
#else
	if (!strncmp(prefix, "wl0", 3))
		tcapi_get(WLAN_COMMON_NODE, "wl0_MacAddress", hwaddr);
	else if (!strncmp(prefix, "wl1", 3))
		tcapi_get(WLAN_COMMON_NODE, "wl1_MacAddress", hwaddr);
	else
		return -1;

	subunit = (prefix[3] == '.') ? atoi(prefix + 4) : -1;

	if (subunit > 0) {
		ether_atoe(hwaddr, ea);
		snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", 
									ea[0], ea[1], ea[2], ea[3], ea[4], ea[5] + subunit);
	}
	else
		snprintf(buf, sizeof(buf), "%s", hwaddr);

	*value = strdup(buf);
	return *value ? 0 : -1;		
#endif
}

static int get_wlan_channel(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, buf[32] = {0}, tmp[32] = {0};
	int unit;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

	/* allow guest networks */
	if (strncmp(prefix, "wl0", 3) == 0)
		unit = 0;
	else if (strncmp(prefix, "wl1", 3) == 0)
		unit = 1;
	else
		return -1;

	snprintf(buf, sizeof(buf), "%u", wl_control_channel(unit));

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_wlan_channel(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
#if defined(RTCONFIG_BCMWL6) || defined(DSL_ASUSWRT)
	char buf[32] = {0};
#endif
	int unit;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

	/* no guest networks */
	if (strncmp(prefix, "wl0_", 4) == 0)
		unit = 0;
	else if (strncmp(prefix, "wl1_", 4) == 0)
		unit = 1;
	else
		return -1;

#ifdef ASUSWRT
#if ( !defined RTCONFIG_QCA)  && (!defined RTCONFIG_RALINK)
	if (strstr(nvram_safe_get(strcat_r(prefix, "chlist", tmp)), value) == NULL)
		return -1;
#endif
#ifdef RTCONFIG_BCMWL6
	snprintf(buf, sizeof(buf), "%s", value);

	if ((unit == 0) &&
	    (nvram_get_int("wl0_bw") == 0 || nvram_get_int("wl0_bw") == 2)) {
		int channel = atoi(value);
		if (channel >= 1 && channel <= 4)
			strcat(buf, "l");
		else if (channel >= 8 && channel <= 11)
			strcat(buf, "u");
		else
			strcat(buf, "l");
	}
	return set_nvram(strcat_r(prefix, "chanspec", tmp), buf);
#else
	return set_nvram(strcat_r(prefix, "channel", tmp), value);
#endif
#else
	return set_nvram(WLAN_COMMON_NODE, strcat_r(prefix, "Channel", tmp), buf);
#endif
}

static int get_wlan_name(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char buf[32] = {0};
#endif

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	return get_nvram(strcat_r(prefix, "ifname", tmp), value);
#else
	snprintf(buf, sizeof(buf), "%s", get_wifi_ifname(prefix, tmp));

	*value = strdup(buf);
	return *value ? 0 : -1;
#endif
}

static int get_wlan_autochannel_enable(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
#ifdef RTCONFIG_BCMWL6
	*value = nvram_get_int(strcat_r(prefix, "chanspec", tmp)) ? "false" : "true";
#else
	*value = nvram_get_int(strcat_r(prefix, "channel", tmp)) ? "false" : "true";
#endif
#else
	*value = tcapi_get_int(WLAN_COMMON_NODE, strcat_r(prefix, "Channel", tmp)) ? "false" : "true";
#endif

	*value = strdup(*value);
	return *value ? 0 : -1;
}

static int set_wlan_autochannel_enable(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")], tmp[32];
	char buf[32];
	int unit, res;

	char *path = lib_node2path(node);
	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

	/* no guest networks */
	if (strncmp(prefix, "wl0", 3) == 0)
		unit = 0;
	else if (strncmp(prefix, "wl1", 3) == 0)
		unit = 1;
	else
		return -1;

	res = string2boolean(value);
	if (res == BOOLEAN_ERROR)
		return -1;

	if (string2boolean(value) == BOOLEAN_FALSE) {
#ifdef ASUSWRT
		if (unit == 0)
			nvram_set_int(strcat_r(prefix, "bw", tmp), 1);
#endif
		snprintf(buf, sizeof(buf), "%u",  wl_control_channel(unit));
	} else
		snprintf(buf, sizeof(buf), "%d", 0);

#ifdef ASUSWRT
#ifdef RTCONFIG_BCMWL6
	return set_nvram(strcat_r(prefix, "chanspec", tmp), buf);
#else
	return set_nvram(strcat_r(prefix, "channel", tmp), buf);
#endif
#else
	return set_nvram(WLAN_COMMON_NODE, strcat_r(prefix, "Channel", tmp), buf);
#endif
}

static int get_wlan_ssid(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	return get_nvram(strcat_r(prefix, "ssid", tmp), value);
#else
	return get_nvram(WLAN_NODE, strcat_r(prefix, "ssid", tmp), value);
#endif
}

static int set_wlan_ssid(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	return set_nvram(strcat_r(prefix, "ssid", tmp), value);
#else
	return set_nvram(WLAN_NODE, strcat_r(prefix, "ssid", tmp), value);
#endif
}

static int get_wlan_beacontype(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *auth_mode_x = NULL;
#else
	char auth_mode_x[16] = {0};
#endif
	char *path = lib_node2path(node);
	char *type = "";

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	auth_mode_x = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));

	if (!strcmp(auth_mode_x, "open") || !strcmp(auth_mode_x, "shared")) {
		if (nvram_get_int(strcat_r(prefix, "wep_x", tmp)) == 0)
			type = "None";
		else
			type = "Basic";
	}
	else if (!strcmp(auth_mode_x, "psk"))
		type = "WPA";
	else if (!strcmp(auth_mode_x, "psk2"))
		type = "11i";
	else if (!strcmp(auth_mode_x, "pskpsk2"))
		type = "WPAand11i";
#else
	tcapi_get(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), auth_mode_x);

	if (!strcmp(auth_mode_x, "OPEN") || !strcmp(auth_mode_x, "SHARED")) {
		if (tcapi_get_int(WLAN_NODE, strcat_r(prefix, "wep_x", tmp)) == 0)
			type = "None";
		else
			type = "Basic";
	}
	else if (!strcmp(auth_mode_x, "WPAPSK"))
		type = "WPA";
	else if (!strcmp(auth_mode_x, "WPA2PSK"))
		type = "11i";
	else if (!strcmp(auth_mode_x, "WPAPSKWPA2PSK"))
		type = "WPAand11i";	
#endif

	*value = strdup(type);
	return *value ? 0 : -1;
}

static int set_wlan_beacontype(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0}, buf[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

	memset(buf, 0, sizeof(buf));
#ifdef ASUSWRT
	if (!strcmp(value, "None")) {
		snprintf(buf, sizeof(buf), "open");
		set_nvram(strcat_r(prefix, "wep_x", tmp), "0");
	}
	else if (!strcmp(value, "Basic"))
		snprintf(buf, sizeof(buf), "shared");
	else if (!strcmp(value, "WPA"))
		snprintf(buf, sizeof(buf), "psk");
	else if (!strcmp(value, "11i"))
		snprintf(buf, sizeof(buf), "psk2");
	else if (!strcmp(value, "WPAand11i"))
		snprintf(buf, sizeof(buf), "pskpsk2");
	else
		return -1;

	return set_nvram(strcat_r(prefix, "auth_mode_x", tmp), buf);
#else
	if (!strcmp(value, "None")) {
		snprintf(buf, sizeof(buf), "OPEN");
		set_nvram(WLAN_NODE, strcat_r(prefix, "wep_x", tmp), "0");
	}
	else if (!strcmp(value, "Basic"))
		snprintf(buf, sizeof(buf), "SHARED");
	else if (!strcmp(value, "WPA"))
		snprintf(buf, sizeof(buf), "WPAPSK");
	else if (!strcmp(value, "11i"))
		snprintf(buf, sizeof(buf), "WPA2PSK");
	else if (!strcmp(value, "WPAand11i"))
		snprintf(buf, sizeof(buf), "WPAPSKWPA2PSK");
	else
		return -1;

	return set_nvram(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), buf);	
#endif
}

static int get_wlan_ssid_enable(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	*value = nvram_get_int(strcat_r(prefix, "closed", tmp)) ? "false" : "true";
#else
	*value = tcapi_get_int(WLAN_NODE, strcat_r(prefix, "HideSSID", tmp)) ? "false" : "true";
#endif

	*value = strdup(*value);
	return *value ? 0 : -1;
}

static int set_wlan_ssid_enable(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int res;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

	res = string2boolean(value);
	if (res == BOOLEAN_ERROR)
		return -1;

#ifdef ASUSWRT
	return set_nvram(strcat_r(prefix, "closed", tmp), (res == BOOLEAN_TRUE) ? "0" : "1");
#else
	return set_nvram(WLAN_NODE, strcat_r(prefix, "HideSSID", tmp), (res == BOOLEAN_TRUE) ? "0" : "1");
#endif
}

static int get_wlan_macaddrcontrolenable(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	*value = nvram_match(strcat_r(prefix, "macmode", tmp), "disabled") ? "false" : "true";
#else
	*value = tcapi_match(ACL_NODE, strcat_r(prefix, "wl_macmode", tmp), "disabled") ? "false" : "true";
#endif

	*value = strdup(*value);
	return *value ? 0 : -1;
}

static int set_wlan_macaddrcontrolenable(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int unit, subunit, res;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

	/* allow guest networks */
	if (strncmp(prefix, "wl0", 3) == 0)
		unit = 0;
	else if (strncmp(prefix, "wl1", 3) == 0)
		unit = 1;
	else
		return -1;
	subunit = (prefix[3] == '.') ? atoi(prefix + 4) : -1;

	res = string2boolean(value);
	if (res == BOOLEAN_ERROR)
		return -1;

	if (subunit > 0) {
		if (res == BOOLEAN_TRUE) {
#ifdef ASUSWRT
			snprintf(tmp, sizeof(tmp), "wl%d_macmode", unit);
			set_nvram(tmp, "allow");
#else
			snprintf(tmp, sizeof(tmp), "wl%d_wl_macmode", unit);
			set_nvram(ACL_NODE, tmp, "allow");
#endif
		}
	}

#ifdef ASUSWRT
	return set_nvram(strcat_r(prefix, "macmode", tmp), (res == BOOLEAN_TRUE) ? "allow" : "disabled");
#else
	return set_nvram(ACL_NODE, strcat_r(prefix, "macmode", tmp), (res == BOOLEAN_TRUE) ? "allow" : "disabled");
#endif
}

static int get_wlan_wepkeyindex(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	return get_nvram(strcat_r(prefix, "key", tmp), value);
#else
	return get_nvram(WLAN_NODE, strcat_r(prefix, "key", tmp), value);
#endif
}

static int set_wlan_wepkeyindex(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int index;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

	index = atoi(value);
	if (index < 1 || index > 4)
		return -1;

#ifdef ASUSWRT
	return set_nvram(strcat_r(prefix, "key", tmp), value);
#else
	return set_nvram(WLAN_NODE, strcat_r(prefix, "key", tmp), value);
#endif
}

static int get_wlan_keypassphrase(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	if (nvram_get_int(strcat_r(prefix, "wep_x", tmp)) == 0) {
#else
	if (tcapi_get_int(WLAN_NODE, strcat_r(prefix, "wep_x", tmp)) == 0) {
#endif
		*value = strdup("");
		return *value ? 0 : -1;
	}

#ifdef ASUSWRT
	return get_nvram(strcat_r(prefix, "phrase_x", tmp), value);
#else
	return get_nvram(WLAN_NODE, strcat_r(prefix, "phrase_x", tmp), value);
#endif
}

static int set_wlan_keypassphrase(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);
	int unit, subunit;

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

	if (strncmp(prefix, "wl0", 3) == 0)
		unit = 0;
	else if (strncmp(prefix, "wl1", 3) == 0)
		unit = 1;
	else
		return -1;
	subunit = (prefix[3] == '.') ? atoi(prefix + 4) : 0;

	generate_wep_key(unit, subunit, value);
#ifdef ASUSWRT
	return set_nvram(strcat_r(prefix, "phrase_x", tmp), value);
#else
	return set_nvram(WLAN_NODE, strcat_r(prefix, "phrase_x", tmp), value);	
#endif
}

static int get_wlan_wepencryptionlevel(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *level = "";
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	switch (nvram_get_int(strcat_r(prefix, "wep_x", tmp)))
#else 	/* DSL_ASUSWRT */
	switch (tcapi_get_int(WLAN_NODE, strcat_r(prefix, "wep_x", tmp)))
#endif
	{
	case 1:
		level = "40-bit";
		break;
	case 2:
		level = "104-bit";
		break;
	default:
		level = "Disabled";
		break;
	}

	*value = strdup(level);
	return *value ? 0 : -1;
}

static int get_wlan_basicencrytionmodes(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *wep = "";
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	switch (nvram_get_int(strcat_r(prefix, "wep_x", tmp)))
#else 	/* DSL_ASUSWRT */
	switch (tcapi_get_int(WLAN_NODE, strcat_r(prefix, "wep_x", tmp)))
#endif
	{
	case 0:
		wep = "None";
		break;
	case 1:
	case 2:
		wep = "WEPEncrytion";
		break;
	}

	*value = strdup(wep);
	return *value ? 0 : -1;
}

static int set_wlan_basicencrytionmodes(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int unit, wep;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

	/* allow guest networks */
	if (strncmp(prefix, "wl0", 3) == 0)
		unit = 0;
	else if (strncmp(prefix, "wl1", 3) == 0)
		unit = 1;
	else
		return -1;

	if (war_strcasecmp(value, "None") == 0)
		wep = 0;
	else if(war_strcasecmp(value, "WEPEncrytion") == 0)
		wep = 2;
	else
		return -1;

#ifdef ASUSWRT
	snprintf(tmp, sizeof(tmp), "wl%d_nmode_x", unit);
	set_nvram(tmp, "2");
	set_nvram(strcat_r(prefix, "auth_mode_x", tmp), wep ? "shared" : "open");

	return set_nvram(strcat_r(prefix, "wep_x", tmp), wep ? "2" : "0");
#else
	snprintf(tmp, sizeof(tmp), "wl%d_WirelessMode", unit);
	if (unit == 0)/* 2.4G */
		set_nvram(WLAN_COMMON_NODE, tmp, "0");
	else if (unit == 1)	/* 5G */
		set_nvram(WLAN_COMMON_NODE, tmp, "2");
	set_nvram(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), wep ? "SHARED" : "OPEN");

	return set_nvram(WLAN_NODE, strcat_r(prefix, "wep_x", tmp), wep ? "2" : "0");
#endif
}

static int get_wlan_basicauthenticationmodes(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *buf = "";
#ifdef ASUSWRT
	char *mode = NULL;
#else
	char mode[16] = {0};
#endif
	char *path = lib_node2path(node);
	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	mode = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));

	if (!strncmp(mode, "open", 4))
		buf = "None";
	else if (!strncmp(mode, "shared", 6))		
		buf = "SharedAuthentication";
#else
	tcapi_get(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), mode);

	if (!strncmp(mode, "OPEN", 4))
		buf = "None";
	else if (!strncmp(mode, "SHARED", 6))		
		buf = "SharedAuthentication";
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_wlan_basicauthenticationmodes(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char buf[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	if (!strcmp(value, "None"))
		snprintf(buf, sizeof(buf), "open");
	else if(!strcmp(value, "SharedAuthentication"))
		snprintf(buf, sizeof(buf), "shared");
	else
		return -1;

	return set_nvram(strcat_r(prefix, "auth_mode_x", tmp), buf);
#else
	if (!strcmp(value, "None"))
		snprintf(buf, sizeof(buf), "OPEN");
	else if(!strcmp(value, "SharedAuthentication"))
		snprintf(buf, sizeof(buf), "SHARED");
	else
		return -1;

	return set_nvram(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), buf);
#endif
}

static int get_wlan_wpaencrytionmodes(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *mode = NULL;
#else
	char mode[16] = {0};
#endif
	char *crypto = "";
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	mode = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));
	//if (strncmp(mode, "wpa", 3) == 0 || strncmp(mode, "psk", 3) == 0) {
	if (strcmp(mode, "wpa") == 0 || strcmp(mode, "psk") == 0 || strcmp(mode, "pskpsk2") == 0) {
		mode = nvram_safe_get(strcat_r(prefix, "crypto", tmp));
		if (strcmp(mode, "tkip") == 0)
			crypto = "TKIPEncrytion";
		else if (strcmp(mode, "aes") == 0)
			crypto = "AESEncrytion";
		else if (strcmp(mode, "tkip+aes") == 0)
			crypto = "TKIPandAESEncrytion";
	}
#else
	tcapi_get(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), mode);

	if (strcmp(mode, "WPA") == 0 || strcmp(mode, "WPAPSK") == 0 || strcmp(mode, "WPAPSKWPA2PSK") == 0) {
		tcapi_get(WLAN_NODE, strcat_r(prefix, "crypto", tmp), mode);
		if (strcmp(mode, "TKIP") == 0)
			crypto = "TKIPEncrytion";
		else if (strcmp(mode, "AES") == 0)
			crypto = "AESEncrytion";
		else if (strcmp(mode, "TKIP+AES") == 0)
			crypto = "TKIPandAESEncrytion";
	}
#endif

	*value = strdup(crypto);
	return *value ? 0 : -1;
}

static int set_wlan_wpaencrytionmodes(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *crypto;
#ifdef ASUSWRT
	char *mode = NULL;
#else
	char mode[16] = {0};
#endif
	int unit;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

	/* allow guest networks */
	if (strncmp(prefix, "wl0", 3) == 0)
		unit = 0;
	else if (strncmp(prefix, "wl1", 3) == 0)
		unit = 1;
	else
		return -1;

#ifdef ASUSWRT
	mode = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));

	if (strcmp(mode, "psk2") == 0) {
		if (war_strcasecmp(value, "AESEncrytion") == 0)
			crypto = "aes";
		else
			return -1;
	}
	else if (strcmp(mode, "pskpsk2") == 0) {
		if (war_strcasecmp(value, "AESEncrytion") == 0)
			crypto = "aes";
		else if (war_strcasecmp(value, "TKIPandAESEncrytion") == 0)
			crypto = "tkip+aes";
		else
			return -1;
	}
	else
		return -1;

	snprintf(tmp, sizeof(tmp), "wl%d_nmode_x", unit);
	set_nvram(tmp, "2");

	return set_nvram(strcat_r(prefix, "crypto", tmp), crypto);

#else 	/* DSL_ASUSWRT */
	tcapi_get(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), mode);

	if (strcmp(mode, "WPA2PSK") == 0) {
		if (war_strcasecmp(value, "AESEncrytion") == 0)
			crypto = "AES";
		else
			return -1;
	}
	else if (strcmp(mode, "WPAPSKWPA2PSK") == 0) {
		if (war_strcasecmp(value, "AESEncrytion") == 0)
			crypto = "AES";
		else if (war_strcasecmp(value, "TKIPandAESEncrytion") == 0)
			crypto = "TKIP+AES";
		else
			return -1;
	}
	else
		return -1;

	snprintf(tmp, sizeof(tmp), "wl%d_WirelessMode", unit);
	if (unit == 0)/* 2.4G */
		set_nvram(WLAN_COMMON_NODE, tmp, "0");
	else if (unit == 1)	/* 5G */
		set_nvram(WLAN_COMMON_NODE, tmp, "2");

	return set_nvram(WLAN_NODE, strcat_r(prefix, "crypto", tmp), crypto);
#endif
}

static int get_wlan_wpaauthenticationmode(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *mode = NULL;
#else
	char mode[16] = {0};
#endif
	char *auth = "";
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	mode = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));
	if (strcmp(mode, "psk") == 0 || strcmp(mode, "pskpsk2") == 0)
		auth = "PSKAuthentication";
	else if (strcmp(auth, "wpa") == 0)
		auth = "EAPAuthentication";
#else
	tcapi_get(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), mode);
	if (strcmp(mode, "WPAPSK") == 0 || strcmp(mode, "WPAPSKWPA2PSK") == 0)
		auth = "PSKAuthentication";
	else if (strcmp(auth, "WPA") == 0)
		auth = "EAPAuthentication";	
#endif

	*value = strdup(auth);
	return *value ? 0 : -1;
}

static int set_wlan_wpaauthenticationmode(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *mode;
	int unit;
#ifdef ASUSWRT
	char *auth_mode = NULL;
#else
	char auth_mode[16] = {0};
#endif
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

	/* allow guest networks */
	if (strncmp(prefix, "wl0", 3) == 0)
		unit = 0;
	else if (strncmp(prefix, "wl1", 3) == 0)
		unit = 1;
	else
		return -1;

#ifdef ASUSWRT
	auth_mode = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));

	if (war_strcasecmp(value, "PSKAuthentication") == 0) {
		if (!strcmp(auth_mode, "pskpsk2"))
			mode = "pskpsk2";
		else
			mode = "psk";
	}
	else if (war_strcasecmp(value, "EAPAuthentication") == 0) {
		if (!strcmp(auth_mode, "wpawpa2"))
			mode = "wpawpa2";
		else
			mode = "wpa";
	}
	else
		return -1;

	snprintf(tmp, sizeof(tmp), "wl%d_nmode_x", unit);
	set_nvram(tmp, "2");

	return set_nvram(strcat_r(prefix, "auth_mode_x", tmp), mode);
#else
	tcapi_get(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), auth_mode);

	if (war_strcasecmp(value, "PSKAuthentication") == 0) {
		if (!strcmp(auth_mode, "WPAPSKWPA2PSK"))
			mode = "WPAPSKWPA2PSK";
		else
			mode = "WPAPSK";
	}
	else if (war_strcasecmp(value, "EAPAuthentication") == 0) {
		if (!strcmp(auth_mode, "WPA1WPA2"))
			mode = "WPA1WPA2";
		else
			mode = "WPA";
	}
	else
		return -1;

	snprintf(tmp, sizeof(tmp), "wl%d_WirelessMode", unit);
	if (unit == 0)/* 2.4G */
		set_nvram(WLAN_COMMON_NODE, tmp, "0");
	else if (unit == 1)	/* 5G */
		set_nvram(WLAN_COMMON_NODE, tmp, "2");

	return set_nvram(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), mode);	
#endif
}

static int get_wlan_ieee11iencryptionmodes(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *mode = NULL;
#else
	char mode[16] = {0};
#endif
	char *crypto = "";
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	mode = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));
	if (strcmp(mode, "wpa2") == 0 ||
	    strcmp(mode, "psk2") == 0 || strcmp(mode, "pskpsk2") == 0) {
		mode = nvram_safe_get(strcat_r(prefix, "crypto", tmp));
		if (strcmp(mode, "tkip") == 0)
			crypto = "TKIPEncrytion";
		else if (strcmp(mode, "aes") == 0)
			crypto = "AESEncrytion";
		else if (strcmp(mode, "tkip+aes") == 0)
			crypto = "TKIPandAESEncrytion";
	}
#else
	tcapi_get(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), mode);
	if (strcmp(mode, "WPA2") == 0 ||
	    strcmp(mode, "WPA2PSK") == 0 || strcmp(mode, "WPAPSKWPA2PSK") == 0) {
		tcapi_get(WLAN_NODE, strcat_r(prefix, "crypto", tmp), mode);
		if (strcmp(mode, "TKIP") == 0)
			crypto = "TKIPEncrytion";
		else if (strcmp(mode, "AES") == 0)
			crypto = "AESEncrytion";
		else if (strcmp(mode, "TKIP+AES") == 0)
			crypto = "TKIPandAESEncrytion";
	}
#endif

	*value = strdup(crypto);
	return *value ? 0 : -1;
}

static int set_wlan_ieee11iencryptionmodes(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *crypto;
#ifdef ASUSWRT
	char *mode = NULL;
#else 	/* DSL_ASUSWRT */
	char mode[16] = {0};
#endif
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	mode = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));

	if (strcmp(mode, "wpa2") == 0) {
		if (war_strcasecmp(value, "AESEncrytion") == 0)
			crypto = "aes";
		else
			return -1;
	}
	else if (strcmp(mode, "wpawpa2") == 0) {
		if (war_strcasecmp(value, "AESEncrytion") == 0)
			crypto = "aes";
		else if (war_strcasecmp(value, "TKIPandAESEncrytion") == 0)
			crypto = "tkip+aes";
		else
			return -1;
	}
	else
		return -1;

	return set_nvram(strcat_r(prefix, "crypto", tmp), crypto);
#else
	tcapi_get(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), mode);

	if (strcmp(mode, "WPA2") == 0) {
		if (war_strcasecmp(value, "AESEncrytion") == 0)
			crypto = "AES";
		else
			return -1;
	}
	else if (strcmp(mode, "WPA1WPA2") == 0) {
		if (war_strcasecmp(value, "AESEncrytion") == 0)
			crypto = "AES";
		else if (war_strcasecmp(value, "TKIPandAESEncrytion") == 0)
			crypto = "TKIP+AES";
		else
			return -1;
	}
	else
		return -1;

	return set_nvram(WLAN_NODE, strcat_r(prefix, "crypto", tmp), crypto);
#endif
}

static int get_wlan_ieee11iauthenticationmode(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *mode = NULL;
#else
	char mode[16] = {0};
#endif
	char *auth = "";
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	mode = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));
	if (strcmp(mode, "psk2") == 0 || strcmp(mode, "pskpsk2") == 0)
		auth = "PSKAuthentication";
	else if (strcmp(auth, "wpa2") == 0)
		auth = "EAPAuthentication";
#else
	tcapi_get(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), mode);
	if (strcmp(mode, "WPA2PSK") == 0 || strcmp(mode, "WPAPSKWPA2PSK") == 0)
		auth = "PSKAuthentication";
	else if (strcmp(auth, "WPA2") == 0)
		auth = "EAPAuthentication";
#endif

	*value = strdup(auth);
	return *value ? 0 : -1;
}

static int set_wlan_ieee11iauthenticationmode(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *mode;
#ifdef ASUSWRT
	char *auth_mode = NULL;
#else
	char auth_mode[16] = {0};
#endif
	char *path = lib_node2path(node);
	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	auth_mode = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));
	if (war_strcasecmp(value, "PSKAuthentication") == 0) {
		if (!strcmp(auth_mode, "pskpsk2"))
			mode= "pskpsk2";
		else
			mode = "psk2";
	}
	else if (war_strcasecmp(value, "EAPAuthentication") == 0) {
		if (!strcmp(auth_mode, "wpawpa2"))
			mode= "wpawpa2";
		else
			mode = "wpa2";
	}
	else
		return -1;

	return set_nvram(strcat_r(prefix, "auth_mode_x", tmp), mode);
#else
	tcapi_get(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), auth_mode);
	if (war_strcasecmp(value, "PSKAuthentication") == 0) {
		if (!strcmp(auth_mode, "WPAPSKWPA2PSK"))
			mode= "WPAPSKWPA2PSK";
		else
			mode = "WPA2PSK";
	}
	else if (war_strcasecmp(value, "EAPAuthentication") == 0) {
		if (!strcmp(auth_mode, "WPA1WPA2"))
			mode= "WPA1WPA2";
		else
			mode = "WPA2";
	}
	else
		return -1;

	return set_nvram(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), mode);	
#endif
}

static int get_wlan_radioenable(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int unit, subunit;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

	/* allow guest networks */
	if (strncmp(prefix, "wl0", 3) == 0)
		unit = 0;
	else if (strncmp(prefix, "wl1", 3) == 0)
		unit = 1;
	else
		return -1;
	subunit = (prefix[3] == '.') ? atoi(prefix + 4) : -1;

#ifdef ASUSWRT
	if (subunit > 0)
		*value = nvram_get_int(strcat_r(prefix, "bss_enabled", tmp)) ? "true" : "false";
	else
		*value = nvram_get_int(strcat_r(prefix, "radio", tmp)) ? "true" : "false";
#else
	if (subunit > 0)
		*value = tcapi_get_int(WLAN_NODE, strcat_r(prefix, "bss_enabled", tmp)) ? "true" : "false";
	else
		*value = tcapi_get_int(WLAN_NODE, strcat_r(prefix, "radio_on", tmp)) ? "true" : "false";	
#endif

	*value = strdup(*value);
	return *value ? 0 : -1;
}

static int set_wlan_radioenable(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int unit, subunit, res;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

	/* allow guest networks */
	if (strncmp(prefix, "wl0", 3) == 0)
		unit = 0;
	else if (strncmp(prefix, "wl1", 3) == 0)
		unit = 1;
	else
		return -1;
	subunit = (prefix[3] == '.') ? atoi(prefix + 4) : -1;

	res = string2boolean(value);
	if (res == BOOLEAN_ERROR)
		return -1;

#ifdef ASUSWRT
	if (subunit > 0) {
		if (res == BOOLEAN_TRUE) {
			nvram_set_int(strcat_r(prefix, "radio", tmp), 1);
			nvram_set_int(strcat_r(prefix, "mbss", tmp), 1);
		}
		res = set_nvram(strcat_r(prefix, "bss_enabled", tmp), (res == BOOLEAN_TRUE) ? "1" : "0");
	} else
		res = set_nvram(strcat_r(prefix, "radio", tmp), (res == BOOLEAN_TRUE) ? "1" : "0");
#else
	if (subunit > 0)
		res = set_nvram(WLAN_NODE, strcat_r(prefix, "bss_enabled", tmp), (res == BOOLEAN_TRUE) ? "1" : "0");
	else
		res = set_nvram(WLAN_NODE, strcat_r(prefix, "radio_on", tmp), (res == BOOLEAN_TRUE) ? "1" : "0");
#endif

	return res;
}

static int get_wlan_totalassociations(node_t node, char *arg, char **value)
{
	char buf[32] = {0};
	char *path = lib_node2path(node);

	snprintf(buf, sizeof(buf), "%d", get_wireless_totalassociations(path));

	*value = strdup(buf);
	return *value ? 0 : -1;	
}

static int get_wlan_wpsenable(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);
#ifdef ASUSWRT	
	char *wpsmode = NULL;
#endif

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	wpsmode = nvram_safe_get(strcat_r(prefix, "wps_mode", tmp));

	if(strncmp(wpsmode, "disabled", 8) == 0)
		snprintf(buf, sizeof(buf), "%s", "false");
	else
		snprintf(buf, sizeof(buf), "%s", "true");
#else
	snprintf(buf, sizeof(buf), "%s", tcapi_get_int(WLAN_NODE, strcat_r(prefix, "wps_enable", tmp)) ? "true" : "false");
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_wlan_wpsenable(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int res = 0;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	if(!war_strcasecmp(value, "1") || !war_strcasecmp(value, "true"))
		res = set_nvram(strcat_r(prefix, "wps_mode", tmp), "enabled");
	else
		res = set_nvram(strcat_r(prefix, "wps_mode", tmp), "disabled");
#else
	if(!war_strcasecmp(value, "1") || !war_strcasecmp(value, "true"))
		res = set_nvram(WLAN_COMMON_NODE, strcat_r(prefix, "wps_enable", tmp), "1");
	else
		res = set_nvram(WLAN_COMMON_NODE, strcat_r(prefix, "wps_enable", tmp), "0");
#endif

	return res;
}

static int get_wlan_wpsdevicepassword(node_t node, char *arg, char **value)
{
	char buf[32] = {0};

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%u", atoi(nvram_safe_get("wps_device_pin")) );
#else
	char prefix[16] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

	snprintf(buf, sizeof(buf), "%u", tcapi_get_int(WLAN_NODE, strcat_r(prefix, "WscVendorPinCode", tmp)));
#endif
	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_wlan_wpsdevicepassword(node_t node, char *arg, char *value)
{
	int res = 0;

#ifdef ASUSWRT
	res = set_nvram("wps_device_pin", value);
#else
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

	res = set_nvram(WLAN_NODE, strcat_r(prefix, "WscVendorPinCode", tmp), value);
#endif

	return res;
}

static int get_wlan_wpsconfigmethodsenabled(node_t node, char *arg, char **value)
{
	char buf[32] = {0};

	snprintf(buf, sizeof(buf), "%s", "PushButton");

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_wlan_wpsconfigmethodsenabled(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int res = 0;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

	if(strcmp(value,"PushButton") == 0)
	{
#ifdef ASUSWRT
		set_nvram("wps_enable_x", "1");
		set_nvram("wps_sta_pin", "00000000");

		res = set_nvram(strcat_r(prefix, "wps_mode", tmp), "enabled");
#else
		if (tcapi_get_int(WLAN_NODE, strcat_r(prefix, "wps_enable", tmp))) {
			set_nvram("Info_WLan", "WPSActiveStatus", "1");
			set_nvram(WLAN_NODE, strcat_r(prefix, "WPSConfMode", tmp), "7");
			set_nvram(WLAN_NODE, strcat_r(prefix, "WPSMode", tmp), "0");
			res = set_nvram(WLAN_NODE, strcat_r(prefix, "WPSConfStatus", tmp), "2");
		}
#endif
	}

	return res;
}

static int get_wlan_wpsconfigurationstate(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%s", (atoi(nvram_safe_get(strcat_r(prefix, "wps_config_state", tmp))) == 0) ? "Not configured" : "Configured");
#else
	snprintf(buf, sizeof(buf), "%s", (tcapi_get_int(WLAN_NODE, strcat_r(prefix, "WPSConfStatus", tmp)) == 1) ? "Not configured" : "Configured");
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_wlan_wep_key(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char key[5] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

	if (strstr(path, "WEPKey.1.WEPKey"))
		sprintf(key, "key%s", "1");
	else if (strstr(path, "WEPKey.2.WEPKey"))
		sprintf(key, "key%s", "2");
	else if (strstr(path, "WEPKey.3.WEPKey"))
		sprintf(key, "key%s", "3");
	else if (strstr(path, "WEPKey.4.WEPKey"))
		sprintf(key, "key%s", "4");
	else
		return -1;

#ifdef ASUSWRT
	return get_nvram(strcat_r(prefix, key, tmp), value);
#else
	return get_nvram(WLAN_NODE, strcat_r(prefix, key, tmp), value);
#endif
}

static int set_wlan_wep_key(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char key[5] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

	if (strstr(path, "WEPKey.1.WEPKey"))
		sprintf(key, "key%s", "1");
	else if (strstr(path, "WEPKey.2.WEPKey"))
		sprintf(key, "key%s", "2");
	else if (strstr(path, "WEPKey.3.WEPKey"))
		sprintf(key, "key%s", "3");
	else if (strstr(path, "WEPKey.4.WEPKey"))
		sprintf(key, "key%s", "4");
	else
		return -1;

#ifdef ASUSWRT
	return set_nvram(strcat_r(prefix, key, tmp), value);
#else
	return set_nvram(WLAN_NODE, strcat_r(prefix, key, tmp), value);
#endif
}

static int get_wlan_presharedkey(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	return get_nvram(strcat_r(prefix, "wpa_psk", tmp), value);
#else
	return get_nvram(WLAN_NODE, strcat_r(prefix, "wpa_psk", tmp), value);
#endif
}

static int set_wlan_presharedkey(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	return set_nvram(strcat_r(prefix, "wpa_psk", tmp), value);
#else
	return set_nvram(WLAN_NODE, strcat_r(prefix, "wpa_psk", tmp), value);	
#endif
}

static int get_wlan_allowedmacaddress(node_t node, char *arg, char **value)
{
	char buf[256] = {0};
	int i = 0;
#ifdef ASUSWRT
	char *maclist_2G = nvram_safe_get("wl0_maclist_x");
	char *maclist_5G = nvram_safe_get("wl1_maclist_x");

	if(strlen(maclist_2G) == 0 && strlen(maclist_5G) == 0)
		sprintf(buf, "%s", "00:00");
	else if(strlen(maclist_2G) != 0 && strlen(maclist_5G) == 0)
		sprintf(buf, "%s,00:00", maclist_2G + 1);
	else if(strlen(maclist_2G) == 0 && strlen(maclist_5G) != 0)
		sprintf(buf, "00:00%s", maclist_5G);
	else if(strlen(maclist_2G) != 0 && strlen(maclist_5G) != 0)
		sprintf(buf, "%s,00:00%s", maclist_2G + 1, maclist_5G);
#else
	char maclist_2G[256] = {0};
	char maclist_5G[256] = {0};

	tcapi_get(ACL_NODE, "wl0_wl_maclist", maclist_2G);
#ifdef TCSUPPORT_DUAL_WLAN
	tcapi_get(ACL_NODE, "wl1_wl_maclist", maclist_5G);
#endif

	if(strlen(maclist_2G) == 0 && strlen(maclist_5G) == 0)
		sprintf(buf, "%s", "00:00");
	else if(strlen(maclist_2G) != 0 && strlen(maclist_5G) == 0)
		sprintf(buf, "%s,00:00", maclist_2G + 1);
	else if(strlen(maclist_2G) == 0 && strlen(maclist_5G) != 0)
		sprintf(buf, "00:00%s", maclist_5G);
	else if(strlen(maclist_2G) != 0 && strlen(maclist_5G) != 0)
		sprintf(buf, "%s,00:00%s", maclist_2G + 1, maclist_5G);
#endif

	for(i = 0; i < strlen(buf); i++)
	{
		if(buf[i] == '<')
			buf[i] = ',';
	}

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_wlan_allowedmacaddress(node_t node, char *arg, char *value)
{
	char tmp[256] = {0};
	int res = 0, i = 0;

	for(i = 0; i < strlen(value); i++){
		if(value[i] == ',')
			value[i] = '<';
	}

	sprintf(tmp, "<%s", value);

	if(strlen(tmp) % 18 != 0)
		return -1;

#ifdef ASUSWRT
	res = set_nvram("wl0_maclist_x", tmp);
	res = set_nvram("wl1_maclist_x", tmp);
#else 	/* DSL_ASUSWRT */
	res = set_nvram(ACL_NODE, "wl0_wl_maclist", tmp);
#ifdef TCSUPPORT_DUAL_WLAN
	res = set_nvram(ACL_NODE, "wl1_wl_maclist", tmp);
#endif
#endif

	return res;
}

static int get_eth_transporttype(node_t node, char *arg, char **value)
{
	char buf[32], prefix[16], tmp[32];
#ifdef ASUSWRT
	char *wan_proto = NULL;
#endif		
	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));
	snprintf(prefix, sizeof(prefix), "%s", eth_wanip_prefix_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;

	memset(buf, 0, sizeof(buf));

#ifdef ASUSWRT
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	if(!strcmp(wan_proto, "pppoe"))
		snprintf(buf, sizeof(buf), "PPPoE");
	else if(!strcmp(wan_proto, "pptp"))
		snprintf(buf, sizeof(buf), "PPTP");
	else if(!strcmp(wan_proto, "l2tp"))
		snprintf(buf, sizeof(buf), "L2TP");
	else
		snprintf(buf, sizeof(buf), "PPPoE");
#else /* DSL_ASUSWRT */
	snprintf(buf, sizeof(buf), "PPPoE");
#endif		
	
	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_eth_dnsservers(node_t node, char *arg, char *value)
{
	char prefix[16] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *wan_proto = NULL, *wan_dnsenable = NULL;
#else 	/* DSL_ASUSWRT */
	char pvc[32] = {0};
#endif
	char *dns = NULL, *first = NULL, *next = NULL;
	int res = 0, i = 0;
	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));
	snprintf(prefix, sizeof(prefix), "%s", eth_wanip_prefix_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	wan_dnsenable = nvram_safe_get(strcat_r(prefix, "dnsenable_x", tmp));

	if(strstr(path, "WANIPConnection") != NULL) {	/* WANIPConnection */
		if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp")
#ifdef RTCONFIG_XDSL
			|| !strcmp(wan_proto, "pppoa")
#endif	
			) {
			if(!strcmp(wan_dnsenable, "1"))	/* can't be modified */
				return -1;
			else
			{
				first = next = strdup(value);
				i = 0;
				while((dns = strsep(&next, ","))) {
					if(*dns) {
#ifdef RTCONFIG_XDSL
						if (check_dsl_wan_device_by_path(path))
							res = (i == 0) ? set_nvram("dslx_dns1", dns) : set_nvram("dslx_dns2", dns);
						else
#endif
						res = (i == 0) ? set_nvram(strcat_r(prefix, "dns1_x", tmp), dns) : 
									set_nvram(strcat_r(prefix, "dns2_x", tmp), dns);
						i++;
					}
					
					if(i == 2)
						break;
				}
				free(first);
			}
		}
		else
		{
			if((!strcmp(wan_proto, "dhcp") && !strcmp(wan_dnsenable, "1"))
#ifdef RTCONFIG_XDSL
				|| (!strcmp(wan_proto, "mer") && !strcmp(wan_dnsenable, "1"))
#endif				
			)	/* can't be modified */
				return -1;
			else
			{
				first = next = strdup(value);
				i = 0;
				while((dns = strsep(&next, ","))) {
					if(*dns) {
#ifdef RTCONFIG_XDSL
						if (check_dsl_wan_device_by_path(path))
							res = (i == 0) ? set_nvram("dslx_dns1", dns) : set_nvram("dslx_dns2", dns);
						else
#endif
						res = (i == 0) ? set_nvram(strcat_r(prefix, "dns1_x", tmp), dns) : 
									set_nvram(strcat_r(prefix, "dns2_x", tmp), dns);
						i++;
					}

					if(i == 2)	/* just need two dns servers */
						break;
				}
				free(first);
			}
		}
	}
	else	/* WANPPPConnection */		/* can't be modified */
		return -1;

#else 	/* DSL_ASUSWRT */
	if(strstr(path, "WANIPConnection") != NULL) {	/* WANIPConnection */
		snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
		if (tcapi_match(pvc, "ISP", "0")) {	/* dhcp */
			if (tcapi_match(pvc, "DNS_type", "1")) {	/* dns set by manual */	
				first = next = strdup(value);
				i = 0;
				while((dns = strsep(&next, ","))) {
					if(*dns) {
						res = (i == 0) ? set_nvram(pvc, "Primary_DNS", dns) : 
									set_nvram(pvc, "Secondary_DNS", dns);
						i++;
					}

					if(i == 2)	/* just need two dns servers */
						break;
				}
				free(first);
			}
			else 	/* dns set by auto */
				return -1;	/* can't be modified */
		}
		else if (tcapi_match(pvc, "ISP", "1")) { 	/* static */
			first = next = strdup(value);
			i = 0;
			while((dns = strsep(&next, ","))) {
				if(*dns) {
					res = (i == 0) ? set_nvram(pvc, "Primary_DNS", dns) : 
								set_nvram(pvc, "Secondary_DNS", dns);
					i++;
				}

				if(i == 2)	/* just need two dns servers */
					break;
			}
			free(first);
		}
		else
			return -1;
	}
	else	/* WANPPPConnection */		/* can't be modified */
		return -1;
#endif

	return res;
}

#endif	/* end of TR098 */

static int get_user_passwd(node_t node, char *arg, char **value)
{
    char buf[32] = {0};

#ifdef ASUSWRT
    snprintf(buf, sizeof(buf), "%s", nvram_safe_get("http_passwd"));
#else 	/* DSL_ASUSWRT */
    char tmp[32] = {0};

    snprintf(buf, sizeof(buf), "%s", tcapi_get_string(ACCOUNT_NODE, "web_passwd", tmp));

#endif
    *value = strdup(buf);
    return *value ? 0 : -1;
}

static int set_user_passwd(node_t node, char *arg, char *value)
{
    int res;

#ifdef ASUSWRT
    res = set_nvram("http_passwd", value);
#else 	/* DSL_ASUSWRT */
    res = set_nvram(ACCOUNT_NODE, "console_passwd", value);
    if (res == 0)
        res = set_nvram(ACCOUNT_NODE, "web_passwd", value);
#endif

    return res;
}

/* get memory info 1:total memory 2: free memory */
unsigned int get_memory_info(int field)
{
	FILE *fp;
	char buf[256] = {0}, data[10] = {0};
	char *ptr = NULL;
	int res = 0, i = 0, fileline = 0;
	unsigned int count = 0;
	
	if((fp = fopen("/proc/meminfo", "r"))==NULL) return 0;

	while(fgets(buf, sizeof(buf), fp) != NULL) {

		i = 0;
		memset(data, 0, sizeof(data));

		fileline++;
		if((ptr = strchr(buf, ':')) == NULL)
			continue;

		ptr++;
		while(*ptr == ' ')
			ptr++;

		while(*ptr != ' '){
			data[i] = *ptr;
			i++;
			ptr++;
		}
		
		i++;
		data[i] = '\0';
		switch(field){
			case 1:
				if(strstr(buf, "MemTotal") != NULL){
					res = sscanf(data, "%u", &count);
					break;
				}
			case 2:
				if(strstr(buf, "MemFree") != NULL){
					res = sscanf(data, "%u", &count);
					break;
				}
		}

		if(fileline == field)
			break;
	}
	fclose(fp);

	return count;
}

static int get_total_memory(node_t node, char *arg, char **value)
{
	char buf[32] = {0};

	snprintf(buf, sizeof(buf), "%u", get_memory_info(1));
	
	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_free_memory(node_t node, char *arg, char **value)
{
	char buf[32] = {0};

	snprintf(buf, sizeof(buf), "%u", get_memory_info(2));
	
	*value = strdup(buf);
	return *value ? 0 : -1;
}

#ifdef TR181	/* start of TR181 */
#ifdef ASUSWRT
int check_path_vaild(char *path)
{
	char **check_path = NULL;
#if defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK) //sherry 2016.2.19  AP mode
	if(sw_mode == SW_MODE_AP)
#else
	if(sw_mode == SW_MODE_AP && nvram_get_int("wlc_psta") != 1)// AP mode
#endif
	{
		char *no_vaild_path[] = {"Device.PPP.", "Device.Routing.", "Device.RouterAdvertisement.", "Device.Ipv6rd.", 				"Device.Hosts.", "Device.NAT.", "Device.DHCPv4.Server.", "Device.Upnp.", "Device.Firewall.", NULL};
		check_path = no_vaild_path;
	}
#if defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK) //sherry 2016.2.19  media bridge mode
	else if(sw_mode == SW_MODE_REPEATER && nvram_get_int("wlc_psta") == 1)
#else
	else if(sw_mode == SW_MODE_AP && nvram_get_int("wlc_psta") == 1)// media bridge mode
#endif
	{
		char *vaild_path1[] = {"Device.DeviceInfo.", "Device.Time.", "Device.ManagementServer.", "Device.GatewayInfo.", "Device.UserInterface.Avail", "Device.UserInterface.Current", "Device.Diagnostics.", "Device.Users.", "Device.BulkData.", NULL};
		check_path = vaild_path1;
	}
#if defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK) //sherry 2016.2.19  repeater mode
	else if(sw_mode == SW_MODE_REPEATER && nvram_get_int("wlc_psta") != 1) 
#else
	else if(sw_mode == SW_MODE_REPEATER) //repeater mode
#endif
	{
		char *vaild_path2[] = {"Device.DeviceInfo.", "Device.Time.", "Device.ManagementServer.", "Device.GatewayInfo.", "Device.UserInterface.Avail", "Device.UserInterface.Current", "Device.Diagnostics.", "Device.Users.", "Device.BulkData.", "Device.WiFi.Radio", "Device.WiFi.SSID", "Device.WiFi.AccessPointNumberOfEntries", "Device.WiFi.AccessPoint.1", "Device.WiFi.AccessPoint.2", NULL};
		check_path = vaild_path2;
	}
#if defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK)
    else if(sw_mode == SW_MODE_ROUTER)
#else
    else if(sw_mode == SW_MODE_ROUTER) //router mode
#endif
    {
        return 0;
    }
	int i = 0;
	
	while(check_path[i]) {
#if defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK) //sherry 2016.2.19 AP mode
		if(sw_mode == SW_MODE_AP && strncmp(path, check_path[i], strlen(check_path[i])) == 0)
#else
		if(sw_mode == SW_MODE_AP && nvram_get_int("wlc_psta") != 1 && strncmp(path, check_path[i], strlen(check_path[i])) == 0)
#endif
			return 1;
#if defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK) //sherry 2016.2.19 media bridge mode
		else if(sw_mode == SW_MODE_REPEATER && nvram_get_int("wlc_psta") == 1 && strncmp(path, check_path[i], strlen(check_path[i])) == 0)
#else
		else if(sw_mode == SW_MODE_AP && nvram_get_int("wlc_psta") == 1 && strncmp(path, check_path[i], strlen(check_path[i])) == 0)
#endif
			return 0;
#if defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK) //sherry 2016.2.19 repeater mode
		else if(sw_mode == SW_MODE_REPEATER && nvram_get_int("wlc_psta") != 1 && strncmp(path, check_path[i], strlen(check_path[i])) == 0)
#else
		else if(sw_mode == SW_MODE_REPEATER && strncmp(path, check_path[i], strlen(check_path[i])) == 0)
#endif
			return 0;
		i++;
	}
#if defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK) //sherry 2016.2.19 AP mode
	if(sw_mode == SW_MODE_AP )
#else
	if(sw_mode == SW_MODE_AP && nvram_get_int("wlc_psta") != 1)
#endif
		return 0;
#if defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK) //sherry 2016.2.19 media bridge mode
	else if((sw_mode == SW_MODE_REPEATER && nvram_get_int("wlc_psta") == 1) || sw_mode == SW_MODE_REPEATER)
#else
	else if((sw_mode == SW_MODE_AP && nvram_get_int("wlc_psta") == 1) || sw_mode == SW_MODE_REPEATER)
#endif
		return 1;
	else
		return -1;
}

static int get_wifi_radio_possiblechannels(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

	if (strncmp(prefix, "wl0_", 4) == 0){
		char *list = nvram_safe_get(strcat_r(prefix, "chlist", tmp));
		if (*(list + strlen(list) - 2) != ' ')
			snprintf(buf, sizeof(buf), "%c-%s", *list, list + strlen(list) - 2);
		else
			snprintf(buf, sizeof(buf), "%c-%s", *list, list + strlen(list) - 1);
	}
	else
		snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "chlist", tmp)));

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_MRU(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wanXXXXXXXXXX_")] = {0}, tmp[32] = {0};

#ifdef RTCONFIG_DUALWAN
	int unit = wan_primary_ifunit();
#else
	int unit = 0;
#endif
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "pppoe_mru", tmp)));

	*value = strdup(buf);
	return *value ? 0 : -1;
}
#endif

static int get_radio_totalbytessent(node_t node, char *arg, char ** value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	long unsigned totalbytes = 0;
	int i = 0;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
#ifdef ASUSWRT
#ifdef RTCONFIG_QTN
	if (!strcmp(prefix, "wl1_"))
		totalbytes = (unsigned long) get_statistic_of_qtn_dev("wifi0", ETHERNET_BYTES_SENT);
	else
#endif
	totalbytes = get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_BYTES_SENT);
#else 	/* DSL_ASUSWRT */
	totalbytes = get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_BYTES_SENT);
#endif

	if(strncmp(prefix, "wl0_", 4) == 0) {	/* for 2.4G */
#ifdef ASUSWRT
		for(i = 0; i < num_of_mssid_support(0); i++) {
			sprintf(prefix, "wl0.%d_", i + 1);
			totalbytes += get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_BYTES_SENT);
		}
#else 	/* DSL_ASUSWRT */
		for(i = 0; i < NUM_MSSID_SUPPORT; i++) {
			sprintf(prefix, "wl0.%d_", i + 1);
			totalbytes += get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_BYTES_SENT);
		}		
#endif
	}
	else {	/* for 5G */
#ifdef ASUSWRT
		for(i = 0; i < num_of_mssid_support(1); i++) {
			sprintf(prefix, "wl1.%d_", i + 1);
#ifdef RTCONFIG_QTN
			totalbytes += (unsigned long) get_statistic_of_qtn_dev(prefix, ETHERNET_BYTES_SENT);
#else
			totalbytes += get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_BYTES_SENT);
#endif
		}
#else 	/* DSL_ASUSWRT */
		for(i = 0; i < NUM_MSSID_SUPPORT; i++) {
			sprintf(prefix, "wl1.%d_", i + 1);
			totalbytes += get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_BYTES_SENT);
		}		
#endif
	}

	snprintf(buf, sizeof(buf), "%lu", totalbytes);
	
	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_radio_totalbytesreceived(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	long unsigned totalbytes = 0;
	int i = 0;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
#ifdef ASUSWRT
#ifdef RTCONFIG_QTN
	if (!strcmp(prefix, "wl1_"))
		totalbytes = (unsigned long) get_statistic_of_qtn_dev("wifi0", ETHERNET_BYTES_RECEIVED);
	else
#endif
	totalbytes = get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_BYTES_RECEIVED);
#else 	/* DSL_ASUSWRT */
	totalbytes = get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_BYTES_RECEIVED);
#endif

	if(strncmp(prefix, "wl0_", 4) == 0) {	/* for 2.4G */
#ifdef ASUSWRT
		for(i = 0; i < num_of_mssid_support(0); i++) {
			sprintf(prefix, "wl0.%d_", i + 1);
			totalbytes += get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_BYTES_RECEIVED);
		}
#else 	/* DSL_ASUSWRT */
		for(i = 0; i < NUM_MSSID_SUPPORT; i++) {
			sprintf(prefix, "wl0.%d_", i + 1);
			totalbytes += get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_BYTES_RECEIVED);
		}		
#endif
	}
	else {	/* for 5G */
#ifdef ASUSWRT
		for(i = 0; i < num_of_mssid_support(1); i++) {
			sprintf(prefix, "wl1.%d_", i + 1);
#ifdef RTCONFIG_QTN
			totalbytes += (unsigned long) get_statistic_of_qtn_dev(prefix, ETHERNET_BYTES_RECEIVED);
#else
			totalbytes += get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_BYTES_RECEIVED);
#endif
		}
#else 	/* DSL_ASUSWRT */
		for(i = 0; i < NUM_MSSID_SUPPORT; i++) {
			sprintf(prefix, "wl1.%d_", i + 1);
			totalbytes += get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_BYTES_RECEIVED);
		}		
#endif
	}

	snprintf(buf, sizeof(buf), "%lu", totalbytes);
	
	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_radio_totalpacketssent(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	long unsigned totalpackets = 0;
	int i = 0;
	char *path = lib_node2path(node);

	sprintf(prefix, "%s", wifinum_prefix_by_path(path, tmp));
#ifdef ASUSWRT
#ifdef RTCONFIG_QTN
	if (!strcmp(prefix, "wl1_"))
		totalpackets = (unsigned long) get_statistic_of_qtn_dev("wifi0", ETHERNET_PACKETS_SENT);
	else
#endif
	totalpackets = get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_PACKETS_SENT);
#else 	/* DSL_ASUSWRT */
	totalpackets = get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_PACKETS_SENT);
#endif

	if(strncmp(prefix, "wl0_", 4) == 0) {	/* for 2.4G */
#ifdef ASUSWRT
		for(i = 0; i < num_of_mssid_support(0); i++) {
			sprintf(prefix, "wl0.%d_", i + 1);
			totalpackets += get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_PACKETS_SENT);
		}
#else 	/* DSL_ASUSWRT */
		for(i = 0; i < NUM_MSSID_SUPPORT; i++) {
			sprintf(prefix, "wl0.%d_", i + 1);
			totalpackets += get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_PACKETS_SENT);
		}		
#endif
	}
	else {	/* for 5G */
#ifdef ASUSWRT
		for(i = 0; i < num_of_mssid_support(1); i++) {
			sprintf(prefix, "wl1.%d_", i + 1);
#ifdef RTCONFIG_QTN
			totalpackets += (unsigned long) get_statistic_of_qtn_dev(prefix, ETHERNET_PACKETS_SENT);
#else
			totalpackets += get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_PACKETS_SENT);
#endif
		}
#else 	/* DSL_ASUSWRT */
		for(i = 0; i < NUM_MSSID_SUPPORT; i++) {
			sprintf(prefix, "wl1.%d_", i + 1);
			totalpackets += get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_PACKETS_SENT);
		}		
#endif
	}

	snprintf(buf, sizeof(buf), "%lu", totalpackets);
	
	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_radio_totalpacketsreceived(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	long unsigned totalpackets = 0;
	int i = 0;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
#ifdef ASUSWRT
#ifdef RTCONFIG_QTN
	if (!strcmp(prefix, "wl1_"))
		totalpackets = (unsigned long) get_statistic_of_qtn_dev("wifi0", ETHERNET_PACKETS_RECEIVED);
	else
#endif
	totalpackets = get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_PACKETS_RECEIVED);
#else 	/* DSL_ASUSWRT */
	totalpackets = get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_PACKETS_RECEIVED);
#endif

	if(strncmp(prefix, "wl0_", 4) == 0) {	/* for 2.4G */
#ifdef ASUSWRT
		for(i = 0; i < num_of_mssid_support(0); i++) {
			sprintf(prefix, "wl0.%d_", i + 1);
			totalpackets += get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_PACKETS_RECEIVED);
		}
#else 	/* DSL_ASUSWRT */
		for(i = 0; i < NUM_MSSID_SUPPORT; i++) {
			sprintf(prefix, "wl0.%d_", i + 1);
			totalpackets += get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_PACKETS_RECEIVED);
		}		
#endif
	}
	else {	/* for 5G */
#ifdef ASUSWRT
		for(i = 0; i < num_of_mssid_support(1); i++) {
			sprintf(prefix, "wl1.%d_", i + 1);
#ifdef RTCONFIG_QTN
			totalpackets += (unsigned long) get_statistic_of_qtn_dev(prefix, ETHERNET_PACKETS_RECEIVED);
#else
			totalpackets += get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_PACKETS_RECEIVED);
#endif
		}
#else 	/* DSL_ASUSWRT */
		for(i = 0; i < NUM_MSSID_SUPPORT; i++) {
			sprintf(prefix, "wl1.%d_", i + 1);
			totalpackets += get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_PACKETS_RECEIVED);
		}		
#endif
	}

	snprintf(buf, sizeof(buf), "%lu", totalpackets);
	
	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_radio_errorssent(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};	
	long unsigned errors = 0;
	int i = 0;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
#ifdef ASUSWRT
#ifdef RTCONFIG_QTN
	if (!strcmp(prefix, "wl1_"))
		errors = (unsigned long) get_statistic_of_qtn_dev("wifi0", ETHERNET_ERRORS_SENT);
	else
#endif
	errors = get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_ERRORS_SENT);
#else 	/* DSL_ASUSWRT */
	errors = get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_ERRORS_SENT);
#endif

	if(strncmp(prefix, "wl0_", 4) == 0) {	/* for 2.4G */
#ifdef ASUSWRT
		for(i = 0; i < num_of_mssid_support(0); i++) {
			sprintf(prefix, "wl0.%d_", i + 1);
			errors += get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_ERRORS_SENT);
		}
#else 	/* DSL_ASUSWRT */
		for(i = 0; i < NUM_MSSID_SUPPORT; i++) {
			sprintf(prefix, "wl0.%d_", i + 1);
			errors += get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_ERRORS_SENT);
		}		
#endif
	}
	else {	/* for 5G */
#ifdef ASUSWRT
		for(i = 0; i < num_of_mssid_support(1); i++) {
			sprintf(prefix, "wl1.%d_", i + 1);
#ifdef RTCONFIG_QTN
			errors += (unsigned long) get_statistic_of_qtn_dev(prefix, ETHERNET_ERRORS_SENT);
#else
			errors += get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_ERRORS_SENT);
#endif
		}
#else 	/* DSL_ASUSWRT */
		for(i = 0; i < NUM_MSSID_SUPPORT; i++) {
			sprintf(prefix, "wl1.%d_", i + 1);
			errors += get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_ERRORS_SENT);
		}		
#endif
	}

	snprintf(buf, sizeof(buf), "%lu", errors);
	
	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_radio_errorsreceived(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};	
	long unsigned errors = 0;
	int i = 0;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
#ifdef ASUSWRT
#ifdef RTCONFIG_QTN
	if (!strcmp(prefix, "wl1_"))
		errors = (unsigned long) get_statistic_of_qtn_dev("wifi0", ETHERNET_ERRORS_RECEIVED);
	else
#endif
	errors = get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_ERRORS_RECEIVED);
#else 	/* DSL_ASUSWRT */
	errors = get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_ERRORS_RECEIVED);
#endif

	if(strncmp(prefix, "wl0_", 4) == 0) {	/* for 2.4G */
#ifdef ASUSWRT
		for(i = 0; i < num_of_mssid_support(0); i++) {
			sprintf(prefix, "wl0.%d_", i + 1);
			errors += get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_ERRORS_RECEIVED);
		}
#else 	/* DSL_ASUSWRT */
		for(i = 0; i < NUM_MSSID_SUPPORT; i++) {
			sprintf(prefix, "wl0.%d_", i + 1);
			errors += get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_ERRORS_RECEIVED);
		}		
#endif
	}
	else {	/* for 5G */
#ifdef ASUSWRT
		for(i = 0; i < num_of_mssid_support(1); i++) {
			sprintf(prefix, "wl1.%d_", i + 1);
#ifdef RTCONFIG_QTN
			errors += (unsigned long) get_statistic_of_qtn_dev(prefix, ETHERNET_ERRORS_RECEIVED);
#else
			errors += get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_ERRORS_RECEIVED);
#endif
		}
#else 	/* DSL_ASUSWRT */
		for(i = 0; i < NUM_MSSID_SUPPORT; i++) {
			sprintf(prefix, "wl1.%d_", i + 1);
			errors += get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_ERRORS_RECEIVED);
		}		
#endif
	}

	snprintf(buf, sizeof(buf), "%lu", errors);
	
	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_radio_discardpacketssent(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	long unsigned discardpackets = 0;
	int i = 0;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
#ifdef ASUSWRT
#ifdef RTCONFIG_QTN
	if (!strcmp(prefix, "wl1_"))
		discardpackets = (unsigned long) get_statistic_of_qtn_dev("wifi0", ETHERNET_DISCARD_PACKETS_SENT);
	else
#endif
	discardpackets = get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_DISCARD_PACKETS_SENT);
#else 	/* DSL_ASUSWRT */
	discardpackets = get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_DISCARD_PACKETS_SENT);
#endif

	if(strncmp(prefix, "wl0_", 4) == 0) {	/* for 2.4G */
#ifdef ASUSWRT
		for(i = 0; i < num_of_mssid_support(0); i++) {
			sprintf(prefix, "wl0.%d_", i + 1);
			discardpackets += get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_DISCARD_PACKETS_SENT);
		}
#else 	/* DSL_ASUSWRT */
		for(i = 0; i < NUM_MSSID_SUPPORT; i++) {
			sprintf(prefix, "wl0.%d_", i + 1);
			discardpackets += get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_DISCARD_PACKETS_SENT);
		}		
#endif
	}
	else {	/* for 5G */
#ifdef ASUSWRT
		for(i = 0; i < num_of_mssid_support(1); i++) {
			sprintf(prefix, "wl1.%d_", i + 1);
#ifdef RTCONFIG_QTN
			discardpackets += (unsigned long) get_statistic_of_qtn_dev(prefix, ETHERNET_DISCARD_PACKETS_SENT);
#else
			discardpackets += get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_DISCARD_PACKETS_SENT);
#endif
		}
#else 	/* DSL_ASUSWRT */
		for(i = 0; i < NUM_MSSID_SUPPORT; i++) {
			sprintf(prefix, "wl1.%d_", i + 1);
			discardpackets += get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_DISCARD_PACKETS_SENT);
		}		
#endif
	}

	snprintf(buf, sizeof(buf), "%lu", discardpackets);
	
	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_radio_discardpacketsreceived(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	unsigned long discardpackets = 0;
	int i = 0;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
#ifdef ASUSWRT
#ifdef RTCONFIG_QTN
	if (!strcmp(prefix, "wl1_"))
		discardpackets = (unsigned long) get_statistic_of_qtn_dev("wifi0", ETHERNET_DISCARD_PACKETS_RECEIVED);
	else
#endif
	discardpackets = get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_DISCARD_PACKETS_RECEIVED);
#else 	/* DSL_ASUSWRT */
	discardpackets = get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_DISCARD_PACKETS_RECEIVED);
#endif

	if(strncmp(prefix, "wl0_", 4) == 0) {	/* for 2.4G */
#ifdef ASUSWRT
		for(i = 0; i < num_of_mssid_support(0); i++) {
			sprintf(prefix, "wl0.%d_", i + 1);
			discardpackets += get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_DISCARD_PACKETS_RECEIVED);
		}
#else 	/* DSL_ASUSWRT */
		for(i = 0; i < NUM_MSSID_SUPPORT; i++) {
			sprintf(prefix, "wl0.%d_", i + 1);
			discardpackets += get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_DISCARD_PACKETS_RECEIVED);
		}		
#endif
	}
	else {	/* for 5G */
#ifdef ASUSWRT
		for(i = 0; i < num_of_mssid_support(1); i++) {
			sprintf(prefix, "wl1.%d_", i + 1);
#ifdef RTCONFIG_QTN
			discardpackets += (unsigned long) get_statistic_of_qtn_dev(prefix, ETHERNET_DISCARD_PACKETS_RECEIVED);
#else
			discardpackets += get_statistic_of_net_dev(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), ETHERNET_DISCARD_PACKETS_RECEIVED);
#endif
		}
#else 	/* DSL_ASUSWRT */
		for(i = 0; i < NUM_MSSID_SUPPORT; i++) {
			sprintf(prefix, "wl1.%d_", i + 1);
			discardpackets += get_statistic_of_net_dev(get_wifi_ifname(prefix, tmp), ETHERNET_DISCARD_PACKETS_RECEIVED);
		}		
#endif
	}

	snprintf(buf, sizeof(buf), "%lu", discardpackets);
	
	*value = strdup(buf);
	return *value ? 0 : -1;
}

/* add client ipaddr and hostname for Device.Hosts.Host */
int add_host_client(char *ipaddr, char *hostname)
{
	char buf[128] = {0}, tmp[64] = {0};
	int i = 0;
	node_t *children = NULL;
	node_t node;
	int count = 0, rec_count = 0, need_add = 1;

	tr_log(LOG_DEBUG, "add_host_client - start");
	tr_log(LOG_DEBUG, "ipaddr: %s, host: %s", ipaddr, hostname);

	if(lib_resolve_node(HOSTS_HOST, &node) == 0) {
		rec_count = count = lib_get_children(node, &children);
		
		while(count > 0) {
			char name[16];
			count--;

			if(lib_get_property(children[count], "name", name) == 0) {
				if(string_is_digits(name) == 1) {
					char ipaddr_inst[65] = {0}, hostname_inst[65] = {0};
					//char ipaddr_buf[128], hostname_buf[128];
 
					/* get the data of IPAddress */
					sprintf(buf, "%s.%s.IPAddress", HOSTS_HOST, name);
					sprintf(ipaddr_inst, "%s", __get_parameter_value(buf, tmp));

					/* get the data of Hostname */
					sprintf(buf, "%s.%s.HostName", HOSTS_HOST, name);
					sprintf(hostname_inst, "%s", __get_parameter_value(buf, tmp));

					if(!strcmp(ipaddr, ipaddr_inst) && !strcmp(hostname, hostname_inst)) {
						need_add = 0;
						break;
					}
				}
			}
		}

		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}
	}

	if(need_add) {
		char count_str[8] = {0};

		memset(buf, 0, sizeof(buf));
		sprintf(buf, "%s.", HOSTS_HOST);
		if((i = add_object(buf, strlen(buf))) < 9000) {
			/* set ipaddr hostname */
			sprintf(buf, "%s.%d.IPAddress", HOSTS_HOST, i);
			__set_parameter_value(buf, ipaddr);

			sprintf(buf, "%s.%d.HostName", HOSTS_HOST, i);		
			__set_parameter_value(buf, hostname);

			/* set the number of HostNumberOfEntries */
			snprintf(buf, sizeof(buf), "%s", HOSTS_NUM);
			sprintf(count_str, "%d", rec_count + 1);
			__set_parameter_value(buf, count_str);

			add_inform_parameter(HOSTS_NUM, 1);
		}
		else
			return -1;
	}

	tr_log(LOG_DEBUG, "add_host_client - end");

	return 0;
}

/* delete client ipaddr and hostname for Device.Hosts.Host */
//int del_host_client(char *ipaddr, char *hostname)
int del_host_client(char *ipaddr)
{
	node_t *children = NULL;
	node_t node;
	int count = 0, rec_count = 0;
	int need_del = 0;
	char buf[128] = {0}, tmp[64] = {0};

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);
	//tr_log(LOG_DEBUG, "ipaddr: %s, host: %s", ipaddr, hostname);

	if(lib_resolve_node(HOSTS_HOST, &node) == 0) {
		rec_count = count = lib_get_children(node, &children);
		
		while(count > 0) {
			char name[16];
			count--;

			if(lib_get_property(children[count], "name", name) == 0) {
				if(string_is_digits(name) == 1) {
					char ipaddr_inst[65];//, hostname_inst[65];
					//char ipaddr_buf[128], hostname_buf[128];
 
					/* get the data of IPAddress */
					sprintf(buf, "%s.%s.IPAddress", HOSTS_HOST, name);
					sprintf(ipaddr_inst, "%s", __get_parameter_value(buf, tmp));

					/* get the data of Hostname */
					//sprintf(buf, "%s.%s.HostName", HOSTS_HOST, name);
					//sprintf(hostname_inst, "%s", __get_parameter_value(buf, tmp));

					//if(!strcmp(ipaddr, ipaddr_inst) && !strcmp(hostname, hostname_inst)) {
					if(!strcmp(ipaddr, ipaddr_inst)) {
						need_del = atoi(name);
						break;
					}
				}
			}
		}

		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}
	}

	if(need_del) {
		char count_str[8] = {0};

		sprintf(buf, "%s.%d.", HOSTS_HOST, need_del);
		tr_log(LOG_DEBUG, "The path for delete: %s", buf);
		delete_object(buf, strlen(buf));

		snprintf(buf, sizeof(buf), "%s", HOSTS_NUM);
		sprintf(count_str, "%d", rec_count - 1);
		__set_parameter_value(buf, count_str);

		add_inform_parameter(HOSTS_NUM, 1);

	}

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

static int get_userinterface_port(node_t node, char *arg, char **value)
{
	char buf[32] = {0};

	if(nvram_contains_word("rc_support", "HTTPS")) {
#ifdef ASUSWRT
		if(nvram_get_int("http_enable") == 0)
			snprintf(buf, sizeof(buf), "%d", nvram_get_int("misc_httpport_x"));
		else
			snprintf(buf, sizeof(buf), "%d", nvram_get_int("misc_httpsport_x"));
#else 	/* DSL_ASUSWRT */
		if(tcapi_get_int(HTTPS_NODE, "http_enable") == 1)
			snprintf(buf, sizeof(buf), "%d", tcapi_get_int(FIREWALL_NODE, "misc_httpport_x"));
		else
			snprintf(buf, sizeof(buf), "%d", tcapi_get_int(FIREWALL_NODE, "misc_httpsport_x"));
#endif
	}
	else {
#ifdef ASUSWRT
		snprintf(buf, sizeof(buf), "%d", nvram_get_int("misc_httpport_x"));
#else 	/* DSL_ASUSWRT */
		snprintf(buf, sizeof(buf), "%d", tcapi_get_int(FIREWALL_NODE, "misc_httpport_x"));
#endif
	}

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_userinterface_port(node_t node, char *arg, char *value)
{
	int res;

	if(nvram_contains_word("rc_support", "HTTPS")) {
#ifdef ASUSWRT
		if(nvram_get_int("http_enable") == 0)
			res = set_nvram("misc_httpport_x", value);
		else
			res = set_nvram("misc_httpsport_x", value);
#else 	/* DSL_ASUSWRT */
		if(tcapi_get_int(HTTPS_NODE, "http_enable") == 1)
			res = set_nvram(FIREWALL_NODE, "misc_httpport_x", value);
		else
			res = set_nvram(FIREWALL_NODE, "misc_httpsport_x", value);
#endif
	}
	else
#ifdef ASUSWRT
		res = set_nvram("misc_httpport_x", value);
#else 	/* DSL_ASUSWRT */
		res = set_nvram(FIREWALL_NODE, "misc_httpport_x", value);
#endif

	return res;
}

static int get_support_protocols(node_t node, char *arg, char **value)
{
	char buf[32] = {0};

	if(nvram_contains_word("rc_support", "HTTPS"))
		snprintf(buf, sizeof(buf), "%s", "HTTP,HTTPS");
	else
		snprintf(buf, sizeof(buf), "%s", "HTTP");

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_userinterface_protocol(node_t node, char *arg, char **value)
{
	char buf[32] = {0};

	if(nvram_contains_word("rc_support", "HTTPS")) {
#ifdef ASUSWRT
		if(nvram_get_int("http_enable") == 0)
#else 	/* DSL_ASUSWRT */
		if(tcapi_get_int(HTTPS_NODE, "http_enable") == 1)	
#endif
			snprintf(buf, sizeof(buf), "%s", "HTTP");
		else
			snprintf(buf, sizeof(buf), "%s", "HTTPS");
	}
	else
		snprintf(buf, sizeof(buf), "%s", "HTTP");

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_userinterface_protocol(node_t node, char *arg, char *value)
{
	int res;

	if(nvram_contains_word("rc_support", "HTTPS")) {
#ifdef ASUSWRT
		if(strcmp(value, "HTTP") == 0)
			res = set_nvram("http_enable", "0");
		else
			res = set_nvram("http_enable", "1");
#else 	/* DSL_ASUSWRT */
		if(strcmp(value, "HTTP") == 0)
			res = set_nvram(HTTPS_NODE, "http_enable", "1");
		else
			res = set_nvram(HTTPS_NODE, "http_enable", "2");
#endif
	}
	else {
		if(strcmp(value, "HTTP") == 0)
#ifdef ASUSWRT
			res = set_nvram("http_enable", "0");
#else 	/* DSL_ASUSWRT */
			res = set_nvram(HTTPS_NODE, "http_enable", "1");
#endif
		else
			res = -1;
	}

	return res;
}

static int get_user_name(node_t node, char *arg, char **value)
{
	char buf[32] = {0};

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get("http_username"));
#else 	/* DSL_ASUSWRT */
	char tmp[32] = {0};

	snprintf(buf, sizeof(buf), "%s", tcapi_get_string(ACCOUNT_NODE, "username", tmp));
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_user_name(node_t node, char *arg, char *value)
{
	int res;

#ifdef ASUSWRT
	if(nvram_contains_word("rc_support", "webdav"))
		res = set_nvram("http_username", value);
	else
		res = -1;
#else 	/* DSL_ASUSWRT */
	res = set_nvram(ACCOUNT_NODE, "username", value);
#endif


	return res;
}



static int get_dhcpv4_client_enable(node_t node, char *arg, char **value)
{
	char prefix[16] = {0}, tmp[32] = {0}, buf[32] = {0};
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char pvc[8] = {0};
#endif

	snprintf(prefix, sizeof(prefix), "%s", dhcpv4_client_by_path(path, tmp));

#ifdef ASUSWRT
	if (!strcmp(prefix, "lan_"))	/* for LAN */
		snprintf(buf, sizeof(buf), "%s", nvram_match(strcat_r(prefix, "proto", tmp), "dhcp") ? "true" : "false");
	else 	/* for WAN */
		snprintf(buf, sizeof(buf), "%s", nvram_match(strcat_r(prefix, "proto", tmp), "dhcp") &&
								nvram_get_int(strcat_r(prefix, "enable", tmp)) ? "true" : "false");
#else 	/* DSL_ASUSWRT */
	snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
	snprintf(buf, sizeof(buf), "%s", tcapi_match(convert_prefix_to_pvc(prefix, tmp), "Active", "Yes") &&
	 							tcapi_match(pvc, "ISP", "0") ? "true" : "false");
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_dhcpv4_client_enable(node_t node, char *arg, char *value)
{
	char prefix[16] = {0}, tmp[32] = {0};
	int res = -1;
	char *path = lib_node2path(node);
	char *value_conv = NULL;
#ifdef DSL_ASUSWRT
	char pvc[8] = {0};
#endif

	value_conv = (!war_strcasecmp(value, "1") || !war_strcasecmp(value, "true")) ? "1" : "0";	

	snprintf(prefix, sizeof(prefix), "%s", dhcpv4_client_by_path(path, tmp));
	res = string2boolean(value);
	if (res == BOOLEAN_ERROR)
		return -1;

#ifdef ASUSWRT
	if (!strcmp(prefix, "lan_"))	/* for LAN */
	{
		if (!war_strcasecmp(value, "1") || !war_strcasecmp(value, "true"))
			res = set_nvram(strcat_r(prefix, "proto", tmp), "dhcp");
		else
			res = set_nvram(strcat_r(prefix, "proto", tmp), "static");
	}
	else
	{
		if (nvram_match(strcat_r(prefix, "proto", tmp), "dhcp"))
			res = set_nvram(strcat_r(prefix, "enable", tmp), value_conv);
	}
#else 	/* DSL_ASUSWRT */
	snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
	if (tcapi_match(pvc, "ISP", "0"))
		res = set_nvram(convert_prefix_to_pvc(prefix, tmp), "Active", !strcmp(value_conv, "1") ? "Yes" : "No");
#endif

	return res;
}

static int get_dhcpv4_client_interface(node_t node, char *arg, char **value)
{
	char prefix[16] = {0}, buf[128] = {0};
	char *path = lib_node2path(node);
	int inst = 0;

	//snprintf(prefix, sizeof(prefix), "%s", dhcpv4_client_by_path(path, tmp));

	if (sw_mode == SW_MODE_ROUTER)
	{

	if (strncmp(path, CLIENT_DHCP_1, strlen(CLIENT_DHCP_1)) == 0) {
#ifdef ASUSWRT
		if (wan_pri) {
#ifdef RTCONFIG_XDSL
#ifdef RTCONFIG_DSL
			if (nvram_match("dslx_transmode", ADSL_TRANS_MODE)) {
				snprintf(prefix, sizeof(prefix), "dsl%d", ADSL_INTERNET_INDEX);
				if ((inst = find_xtm_link(ADSL_ID, prefix)) != 0)
					snprintf(buf, sizeof(buf), "%s.%d", ATM_LINK, inst);
			}
#endif
#ifdef RTCONFIG_VDSL			
			if (nvram_match("dslx_transmode", VDSL_TRANS_MODE)) {
				snprintf(prefix, sizeof(prefix), "dsl%d", VDSL_INTERNET_INDEX);
				if ((inst = find_xtm_link(VDSL_ID, prefix)) != 0)
					snprintf(buf, sizeof(buf), "%s.%d", PTM_LINK, inst);
			}
#endif
#else 	/* #ifdef RTCONFIG_XDSL */
			snprintf(buf, sizeof(buf), "%s.%d", ETH_IF, DEVICE_ETH_WAN_INST);			
		}
#endif

#else 	/* DSL_ASUSWRT */
		if (dsl_pri) {
#ifdef TCSUPPORT_WAN_ATM
			if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", ADSL_TRANS_MODE)) || !support_adsl_vdsl) {
				snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, ADSL_INTERNET_INDEX);
				if ((inst = find_xtm_link(ADSL_ID, prefix)) != 0)
					snprintf(buf, sizeof(buf), "%s.%d", ATM_LINK, inst);
			}
#endif
#ifdef TCSUPPORT_WAN_PTM
			if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", VDSL_TRANS_MODE)) || !support_adsl_vdsl) {
				snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, VDSL_INTERNET_INDEX);
				if ((inst = find_xtm_link(VDSL_ID, prefix)) != 0)
					snprintf(buf, sizeof(buf), "%s.%d", PTM_LINK, inst);
			}
#endif			
		}
		else if (wan_pri)
			snprintf(buf, sizeof(buf), "%s.%d", ETH_IF, DEVICE_ETH_WAN_INST);
#endif

#if defined(ASUSWRT) && defined(RTCONFIG_DUALWAN)
		else if (lan_pri)
			snprintf(buf, sizeof(buf), "%s.%d", ETH_IF, DEVICE_ETH_WANLAN_INST);
#endif
#ifdef RTCONFIG_USB_MODEM
		else if (usb_pri)
			snprintf(buf, sizeof(buf), "%s", USB_IF_WAN);
#endif

		if ((inst = search_ip_ppp_lowerlayer(buf, IP_FLAG)) != 0)
			snprintf(buf, sizeof(buf), "%s.%d", IP_IF, inst);
		else
			return -1;
	}
	else if (strncmp(path, CLIENT_DHCP_2, strlen(CLIENT_DHCP_2)) == 0) {
#ifdef ASUSWRT
		if (wan_sec){
#ifdef RTCONFIG_XDSL
#ifdef RTCONFIG_DSL
			if (nvram_match("dslx_transmode", ADSL_TRANS_MODE)) {
				snprintf(prefix, sizeof(prefix), "dsl%d", ADSL_INTERNET_INDEX);
				if ((inst = find_xtm_link(ADSL_ID, prefix)) != 0)
					snprintf(buf, sizeof(buf), "%s.%d", ATM_LINK, inst);
			}
#endif
#ifdef RTCONFIG_VDSL			
			if (nvram_match("dslx_transmode", VDSL_TRANS_MODE)) {
				snprintf(prefix, sizeof(prefix), "dsl%d", VDSL_INTERNET_INDEX);
				if ((inst = find_xtm_link(VDSL_ID, prefix)) != 0)
					snprintf(buf, sizeof(buf), "%s.%d", PTM_LINK, inst);
			}
#endif

#else 	/* #ifdef RTCONFIG_XDSL */
			snprintf(buf, sizeof(buf), "%s.%d", ETH_IF, DEVICE_ETH_WAN_INST);			
		}
#endif

#else 	/* DSL_ASUSWRT */
		if (dsl_pri) {
#ifdef TCSUPPORT_WAN_ATM
			if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", ADSL_TRANS_MODE)) || !support_adsl_vdsl) {
				snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, ADSL_INTERNET_INDEX);
				if ((inst = find_xtm_link(ADSL_ID, prefix)) != 0)
					snprintf(buf, sizeof(buf), "%s.%d", ATM_LINK, inst);
			}
#endif
#ifdef TCSUPPORT_WAN_PTM
			if ((support_adsl_vdsl && tcapi_match(WAN_COMMON_NODE, "DSLMode", VDSL_TRANS_MODE)) || !support_adsl_vdsl) {
				snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, VDSL_INTERNET_INDEX);
				if ((inst = find_xtm_link(VDSL_ID, prefix)) != 0)
					snprintf(buf, sizeof(buf), "%s.%d", PTM_LINK, inst);
			}
#endif			
		}
		else if (wan_sec)
			snprintf(buf, sizeof(buf), "%s.%d", ETH_IF, DEVICE_ETH_WAN_INST);
#endif

#if defined(ASUSWRT) && defined(RTCONFIG_DUALWAN)
		else if (lan_sec)
			snprintf(buf, sizeof(buf), "%s.%d", ETH_IF, DEVICE_ETH_WANLAN_INST);
#endif
#ifdef RTCONFIG_USB_MODEM
		else if (usb_sec)
			snprintf(buf, sizeof(buf), "%s", USB_IF_WAN);
#endif

		if ((inst = search_ip_ppp_lowerlayer(buf, IP_FLAG)) != 0)
			snprintf(buf, sizeof(buf), "%s.%d", IP_IF, inst);
		else
			return -1;
	}
	else
		return -1;

	}
	else if (sw_mode == SW_MODE_AP)
	{
		char path_buf[128] = {0};

		snprintf(path_buf, sizeof(path_buf), "%s.%d", DEVICE_BRIDGING_BRIDGE_PORT_ONE, mng_port_index);
		if ((inst = search_bridging_lowerlayer(path_buf)) != 0)
			snprintf(buf, sizeof(buf), "%s.%d", IP_IF, inst);
	}

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_dhcpv4_client_interface(node_t node, char *arg, char *value)
{
	return -1;
}

static int get_dhcpv4_client_status(node_t node, char *arg, char **value)
{
	char prefix[16] = {0}, tmp[32] = {0};
	int status = 0, unit = 0;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", dhcpv4_client_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	if (!strcmp(prefix, "wan0_"))
		unit = 0;
	else
		unit = 1;
#else 	/* DSL_ASUSWRT */
	unit = convert_prefix_to_wan_unit(prefix);
#endif

#ifdef ASUSWRT
	if (!strcmp(prefix, "lan_")) {
		if (nvram_match(strcat_r(prefix, "proto", tmp), "dhcp"))
			status = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	}
	else
		status = get_wan_state(unit);
#else 	/* DSL_ASUSWRT */
	snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
	if (tcapi_match(pvc, "ISP", "0"))
		status = get_wan_state(unit);
#endif
		
	*value = (status == WAN_STATE_CONNECTED) ? "Enabled" : "Disabled";

	*value = strdup(*value);
	return *value ? 0 : -1;
}

static int get_dhcpv4_client_ipaddr(node_t node, char *arg, char **value)
{
	char prefix[16] = {0}, tmp[32] = {0}, buf[32] = {0};
	int unit = 0;
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char ipaddr[32] = {0}, pvc[8];
#endif

	snprintf(prefix, sizeof(prefix), "%s", dhcpv4_client_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	if (!strcmp(prefix, "wan0_"))
		unit = 0;
	else
		unit = 1;
#else 	/* DSL_ASUSWRT */
	unit = convert_prefix_to_wan_unit(prefix);
#endif

	if (!strcmp(prefix, "lan_"))	/* for LAN */
	{
#ifdef ASUSWRT
		if (nvram_match(strcat_r(prefix, "proto", tmp), "dhcp") && 
			nvram_get_int(strcat_r(prefix, "state_t", tmp)) == WAN_STATE_CONNECTED)
			snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)));
#else 	/* DSL_ASUSWRT */
		snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
		if (tcapi_match(pvc, "ISP", "0") && 
			tcapi_get_int(WANDUCK_NODE, strcat_r(prefix, "state_t", tmp)) == WAN_STATE_CONNECTED)
			snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "ipaddr", tmp), ipaddr));
#endif
	}	/* for WAN */
	else
	{ 
#ifdef ASUSWRT
		if (nvram_match(strcat_r(prefix, "proto", tmp), "dhcp") && get_wan_state(unit) == WAN_STATE_CONNECTED)
			snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)));
#else 	/* DSL_ASUSWRT */
		snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
		if (tcapi_match(pvc, "ISP", "0") && get_wan_state(unit) == WAN_STATE_CONNECTED)
			snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "ipaddr", tmp), ipaddr));
#endif
	}

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_dhcpv4_client_subnetmask(node_t node, char *arg, char **value)
{
	char prefix[16] = {0}, tmp[32] = {0}, buf[32] = {0};
	int unit = 0;
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char netmask[32] = {0}, pvc[8];
#endif

	snprintf(prefix, sizeof(prefix), "%s", dhcpv4_client_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	if (!strcmp(prefix, "wan0_"))
		unit = 0;
	else
		unit = 1;
#else 	/* DSL_ASUSWRT */
	unit = convert_prefix_to_wan_unit(prefix);
#endif

	if (!strcmp(prefix, "lan_"))	/* for LAN */
	{
#ifdef ASUSWRT
		if (nvram_match(strcat_r(prefix, "proto", tmp), "dhcp") && 
			nvram_get_int(strcat_r(prefix, "state_t", tmp)) == WAN_STATE_CONNECTED)
			snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "netmask", tmp)));
#else 	/* DSL_ASUSWRT */
		snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
		if (tcapi_match(pvc, "ISP", "0") && 
			tcapi_get_int(WANDUCK_NODE, strcat_r(prefix, "state_t", tmp)) == WAN_STATE_CONNECTED)
			snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "netmask", tmp), netmask));
#endif
	}
	else
	{
#ifdef ASUSWRT
		if (nvram_match(strcat_r(prefix, "proto", tmp), "dhcp") && get_wan_state(unit) == WAN_STATE_CONNECTED)
			snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "netmask", tmp)));
#else 	/* DSL_ASUSWRT */
		snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
		if (tcapi_match(pvc, "ISP", "0") && get_wan_state(unit) == WAN_STATE_CONNECTED)
			snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "netmask", tmp), netmask));
#endif
	}

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_dhcpv4_client_iprouters(node_t node, char *arg, char **value)
{
	char prefix[16] = {0}, tmp[32] = {0}, buf[32] = {0};
	int unit = 0;
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char gateway[32] = {0};
#endif

	snprintf(prefix, sizeof(prefix), "%s", dhcpv4_client_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	if (!strcmp(prefix, "wan0_"))
		unit = 0;
	else
		unit = 1;
#else 	/* DSL_ASUSWRT */
	unit = convert_prefix_to_wan_unit(prefix);
#endif

	if (!strcmp(prefix, "lan_"))	/* for LAN */
	{
#ifdef ASUSWRT
		if (nvram_match(strcat_r(prefix, "proto", tmp), "dhcp") && 
			nvram_get_int(strcat_r(prefix, "state_t", tmp)) == WAN_STATE_CONNECTED)
			snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "gateway", tmp)));
#else 	/* DSL_ASUSWRT */
		snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
		if (tcapi_match(pvc, "ISP", "0") && 
			tcapi_get_int(WANDUCK_NODE, strcat_r(prefix, "state_t", tmp)) == WAN_STATE_CONNECTED)
			snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "xgateway", tmp), netmask));
#endif
	}
	else
	{
#ifdef ASUSWRT
		if (nvram_match(strcat_r(prefix, "proto", tmp), "dhcp") && get_wan_state(unit) == WAN_STATE_CONNECTED)
			snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "xgateway", tmp)));
#else 	/* DSL_ASUSWRT */
		snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
		if (tcapi_match(pvc, "ISP", "0") && get_wan_state(unit) == WAN_STATE_CONNECTED)
			snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "gateway", tmp), gateway));
#endif
	}

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_dhcpv4_client_dnsservers(node_t node, char *arg, char **value)
{
	char prefix[16] = {0}, tmp[32] = {0}, buf[32] = {0};
	int unit = 0;
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char dns[32] = {0};
#endif

	snprintf(prefix, sizeof(prefix), "%s", dhcpv4_client_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	if (!strcmp(prefix, "wan0_"))
		unit = 0;
	else
		unit = 1;
#else 	/* DSL_ASUSWRT */
	unit = convert_prefix_to_wan_unit(prefix);
#endif

	if (!strcmp(prefix, "lan_"))	/* for LAN */
	{
#ifdef ASUSWRT
		if (nvram_match(strcat_r(prefix, "proto", tmp), "dhcp") && 
			nvram_get_int(strcat_r(prefix, "state_t", tmp)) == WAN_STATE_CONNECTED)
			snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "dns", tmp)));
#else 	/* DSL_ASUSWRT */
		snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
		if (tcapi_match(pvc, "ISP", "0") && 
			tcapi_get_int(WANDUCK_NODE, strcat_r(prefix, "state_t", tmp)) == WAN_STATE_CONNECTED)
			snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "dns", tmp), netmask));
#endif
	}
	else
	{
#ifdef ASUSWRT
		if (nvram_match(strcat_r(prefix, "proto", tmp), "dhcp") && get_wan_state(unit) == WAN_STATE_CONNECTED)
			snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "dns", tmp)));
#else 	/* DSL_ASUSWRT */
		snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
		if (tcapi_match(pvc, "ISP", "0") && get_wan_state(unit) == WAN_STATE_CONNECTED)
			snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "dns", tmp), dns));
#endif
	}

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_dhcpv4_client_remaintime(node_t node, char *arg, char **value)
{
	char prefix[16] = {0}, tmp[32] = {0}, buf[16] = {0};
	int unit = 0;
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char expires[16] = {0};
#endif

	snprintf(prefix, sizeof(prefix), "%s", dhcpv4_client_by_path(path, tmp));

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	if (!strcmp(prefix, "wan0_"))
		unit = 0;
	else
		unit = 1;
#else 	/* DSL_ASUSWRT */
	unit = convert_prefix_to_wan_unit(prefix);
#endif

	if (!strcmp(prefix, "lan_"))	/* for LAN */
	{
#ifdef ASUSWRT
		if (nvram_match(strcat_r(prefix, "proto", tmp), "dhcp") && 
			nvram_get_int(strcat_r(prefix, "state_t", tmp)) == WAN_STATE_CONNECTED)
			snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "lease", tmp)));
#else 	/* DSL_ASUSWRT */
		snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
		if (tcapi_match(pvc, "ISP", "0") && 
			tcapi_get_int(WANDUCK_NODE, strcat_r(prefix, "state_t", tmp)) == WAN_STATE_CONNECTED)
			snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "expires", tmp), netmask));
#endif
	}
	else
	{
#ifdef ASUSWRT
		if (nvram_match(strcat_r(prefix, "proto", tmp), "dhcp") && get_wan_state(unit) == WAN_STATE_CONNECTED)
			snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "expires", tmp)));
#else 	/* DSL_ASUSWRT */
		snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
		if (tcapi_match(pvc, "ISP", "0") && get_wan_state(unit) == WAN_STATE_CONNECTED)
			snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "expires", tmp), expires));
#endif
	}

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_wifi_radio_enable(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int unit, subunit;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

	/* allow guest networks */
	if (strncmp(prefix, "wl0", 3) == 0)
		unit = 0;
	else if (strncmp(prefix, "wl1", 3) == 0)
		unit = 1;
	else
		return -1;
	subunit = (prefix[3] == '.') ? atoi(prefix + 4) : -1;

#ifdef ASUSWRT
	if (subunit > 0)
		*value = nvram_get_int(strcat_r(prefix, "bss_enabled", tmp)) ? "true" : "false";
	else
		*value = nvram_get_int(strcat_r(prefix, "radio", tmp)) ? "true" : "false";
#else 	/* DSL_ASUSWRT */
	if (subunit > 0)
		*value = tcapi_get_int(WLAN_NODE, strcat_r(prefix, "bss_enabled", tmp)) ? "true" : "false";
	else
		*value = tcapi_get_int(WLAN_NODE, strcat_r(prefix, "radio_on", tmp)) ? "true" : "false";
#endif

	*value = strdup(*value);
	return *value ? 0 : -1;
}

static int set_wifi_radio_enable(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int unit, subunit, res;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

	/* allow guest networks */
	if (strncmp(prefix, "wl0", 3) == 0)
		unit = 0;
	else if (strncmp(prefix, "wl1", 3) == 0)
		unit = 1;
	else
		return -1;
	subunit = (prefix[3] == '.') ? atoi(prefix + 4) : -1;

	res = string2boolean(value);
	if (res == BOOLEAN_ERROR)
		return -1;

#ifdef ASUSWRT
	if (subunit > 0) {
		if (res == BOOLEAN_TRUE) {
			nvram_set_int(strcat_r(prefix, "radio", tmp), 1);
			nvram_set_int(strcat_r(prefix, "mbss", tmp), 1);
		}
		res = set_nvram(strcat_r(prefix, "bss_enabled", tmp), (res == BOOLEAN_TRUE) ? "1" : "0");
	} else
		res = set_nvram(strcat_r(prefix, "radio", tmp), (res == BOOLEAN_TRUE) ? "1" : "0");
#else
	if (subunit > 0)
		res = set_nvram(WLAN_NODE, strcat_r(prefix, "bss_enabled", tmp), (res == BOOLEAN_TRUE) ? "1" : "0");
	else
		res = set_nvram(WLAN_NODE, strcat_r(prefix, "radio_on", tmp), (res == BOOLEAN_TRUE) ? "1" : "0");
#endif


	return res;
}

static int get_wifi_radio_name(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char buf[32] = {0};
#endif

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	return get_nvram(strcat_r(prefix, "ifname", tmp), value);
#else
	snprintf(buf, sizeof(buf), "%s", get_wifi_ifname(prefix, tmp));

	*value = strdup(buf);
	return *value ? 0 : -1;
#endif
}

static int get_wifi_radio_frequencybands(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
	
	if (strcmp(prefix,"wl0_") == 0)
		snprintf(buf, sizeof(buf), "%s", "2.4GHz");
	else
		snprintf(buf, sizeof(buf), "%s", "5GHz");

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_wifi_radio_standards(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
	
	if(strcmp(prefix,"wl0_") == 0)
		snprintf(buf, sizeof(buf), "%s", "b,g,n");
	else
		snprintf(buf, sizeof(buf), "%s", "a,n");

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_usim_enable(node_t node, char *arg, char **value)
{
    char buf[32] = {0};
    char tmp[32] = {0};

#ifdef ASUSWRT
    snprintf(buf, sizeof(buf), "%s", nvram_safe_get("usb_modem_act_sim"));
    snprintf(tmp, sizeof(buf), "%s", nvram_safe_get("g3err_pin"));
#else 	/* DSL_ASUSWRT */
#endif
    if(strcmp(buf,"-1") == 0)
    {
       *value = strdup("None");

    }else if(strcmp(buf,"6") == 0)
    {
       *value = strdup("Avaliable");

    }else if(strcmp(buf,"1") == 0)
    {
       *value = strdup("Valid");

    }else if(strcmp(buf,"-2") == 0 || strcmp(buf,"-10") == 0 )
    {
      *value = strdup("Error");
    }else{
        *value = strdup("UNKnowError");
    }
    return *value ? 0 : -1;
}

static int get_wifi_radio_channel(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

	if(strncmp(prefix, "wl0_", 4) == 0)
		snprintf(buf, sizeof(buf), "%u", wl_control_channel(0));
	else
		snprintf(buf, sizeof(buf), "%u", wl_control_channel(1));

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_wifi_radio_channel(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int res = 0;
	char *path = lib_node2path(node);
#ifdef ASUSWRT
	int wlan2G = 0;
	char chanlist[32] = {0};
#else 	/* DSL_ASUSWRT */
	char buf[32] = {0};
#endif

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	if(strncmp(prefix, "wl0_", 4) == 0)
		wlan2G = 1;

	if (wlan2G == 1) {
		if (strstr(nvram_safe_get(strcat_r(prefix, "chlist", chanlist)), value))
		{
			if ((atoi(nvram_safe_get("wl0_bw")) == 0) || (atoi(nvram_safe_get("wl0_bw")) == 2))
			{
				if (!war_strcasecmp(value, "1") || !war_strcasecmp(value, "2") || !war_strcasecmp(value, "3") || !war_strcasecmp(value, "4")){
#ifdef RTCONFIG_BCMWL6
					res = set_nvram(strcat_r(prefix, "chanspec", tmp), strcat(value,"l"));
#else
					res = set_nvram(strcat_r(prefix, "channel", tmp), value);
#endif				
				}
				else if(!war_strcasecmp(value, "8") || !war_strcasecmp(value, "9") || !war_strcasecmp(value, "10") || !war_strcasecmp(value, "11")){
#ifdef RTCONFIG_BCMWL6
					res = set_nvram(strcat_r(prefix, "chanspec", tmp), strcat(value,"u"));
#else
					res = set_nvram(strcat_r(prefix, "channel", tmp), value);
#endif
				}
				else{
#ifdef RTCONFIG_BCMWL6
					res = set_nvram(strcat_r(prefix, "chanspec", tmp), strcat(value,"l"));
#else
					res = set_nvram(strcat_r(prefix, "channel", tmp), value);
#endif
				}
            		}
			else{
#ifdef RTCONFIG_BCMWL6
				res = set_nvram(strcat_r(prefix, "chanspec", tmp), value);
#else
				res = set_nvram(strcat_r(prefix, "channel", tmp), value);
#endif
			}
		}
		else
			res = -1;
	}
	else
	{
		if (strstr(nvram_safe_get(strcat_r(prefix, "chlist", chanlist)), value))
#ifdef RTCONFIG_BCMWL6
				res = set_nvram(strcat_r(prefix, "chanspec", tmp), value);
#else
				res = set_nvram(strcat_r(prefix, "channel", tmp), value);
#endif
		else
			res = -1;
	}

#else 	/* DSL_ASUSWRT */
	res = set_nvram(WLAN_COMMON_NODE, strcat_r(prefix, "Channel", tmp), buf);
#endif
	return res;
}

static int get_wifi_radio_autochannelenable(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
#ifdef RTCONFIG_BCMWL6
	snprintf(buf, sizeof(buf), "%s", (atoi(nvram_safe_get(strcat_r(prefix, "chanspec", tmp))) == 0) ? "true": "false");
#else
	snprintf(buf, sizeof(buf), "%s", (atoi(nvram_safe_get(strcat_r(prefix, "channel", tmp))) == 0) ? "true": "false");
#endif

#else 	/* DSL_ASUSWRT */
	snprintf(buf, sizeof(buf), "%s", tcapi_get_int(WLAN_COMMON_NODE, strcat_r(prefix, "Channel", tmp)) == 0 ? "true": "false");
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_wifi_radio_autochannelenable(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0}, buf[32] = {0};
	int unit, res;

	char *path = lib_node2path(node);
	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

	/* no guest networks */
	if (strncmp(prefix, "wl0", 3) == 0)
		unit = 0;
	else if (strncmp(prefix, "wl1", 3) == 0)
		unit = 1;
	else
		return -1;

	res = string2boolean(value);
	if (res == BOOLEAN_ERROR)
		return -1;

	if (string2boolean(value) == BOOLEAN_FALSE) {
#ifdef ASUSWRT
		if (unit == 0)
			nvram_set_int(strcat_r(prefix, "bw", tmp), 1);
#endif
		snprintf(buf, sizeof(buf), "%u",  wl_control_channel(unit));
	} else
		snprintf(buf, sizeof(buf), "%d", 0);

#ifdef ASUSWRT
#ifdef RTCONFIG_BCMWL6
	return set_nvram(strcat_r(prefix, "chanspec", tmp), buf);
#else
	return set_nvram(strcat_r(prefix, "channel", tmp), buf);
#endif
#else
	return set_nvram(WLAN_COMMON_NODE, strcat_r(prefix, "Channel", tmp), buf);
#endif
}

static int get_wifi_radio_operatingchannelbandwidth(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int bandwidth;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	bandwidth = atoi(nvram_safe_get(strcat_r(prefix, "bw", tmp)));

#ifndef RTCONFIG_BCMWL6	
	/* 0/1/2 20, 20/40, 40MHz */
	if(bandwidth == 0)
		snprintf(buf, sizeof(buf), "%s", "20MHz");
	else if(bandwidth == 1)
		snprintf(buf, sizeof(buf), "%s", "Auto");
	else if(bandwidth == 2)
		snprintf(buf, sizeof(buf), "%s", "40MHz");
#else
	/* 0/1/2/3 auto/20/40/80MHz */
	if(bandwidth == 0)
		snprintf(buf, sizeof(buf), "%s", "Auto");
	else if(bandwidth == 1)
		snprintf(buf, sizeof(buf), "%s", "20MHz");
	else if(bandwidth == 2)
		snprintf(buf, sizeof(buf), "%s", "40MHz");
	else if(bandwidth == 3)
		snprintf(buf, sizeof(buf), "%s", "80MHz");
#endif

#else 	/* DSL_ASUSWRT */
	bandwidth = tcapi_get_int(WLAN_COMMON_NODE, strcat_r(prefix, "HT_BW", tmp));
	/* 0/1/2 20, 20/40, 40MHz */
	if(bandwidth == 0)
		snprintf(buf, sizeof(buf), "%s", "20MHz");
	else if(bandwidth == 1)
		snprintf(buf, sizeof(buf), "%s", "Auto");
	else if(bandwidth == 2)
		snprintf(buf, sizeof(buf), "%s", "40MHz");
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_wifi_radio_operatingchannelbandwidth(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int res = -1;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
#ifndef RTCONFIG_BCMWL6	
	if(strncmp(value, "20MHz", 5) == 0)
		res = set_nvram(strcat_r(prefix, "bw", tmp), "0");
	else if(strncmp(value, "Auto", 4) == 0)
		res = set_nvram(strcat_r(prefix, "bw", tmp), "1");
	else if(strncmp(value, "40MHz", 5) == 0)
		res = set_nvram(strcat_r(prefix, "bw", tmp), "2");
#else
	if(strncmp(value, "20MHz", 5) == 0)
		res = set_nvram(strcat_r(prefix, "bw", tmp), "1");
	else if(strncmp(value, "Auto", 4) == 0)
		res = set_nvram(strcat_r(prefix, "bw", tmp), "0");
	else if(strncmp(value, "40MHz", 5) == 0)
		res = set_nvram(strcat_r(prefix, "bw", tmp), "2");
	else if(strncmp(value, "80MHz", 5) == 0)
		res = set_nvram(strcat_r(prefix, "bw", tmp), "3");
#endif

#else 	/* DSL_ASUSWRT */
	if(strncmp(value, "20MHz", 5) == 0)
		res = set_nvram(WLAN_COMMON_NODE, strcat_r(prefix, "HT_BW", tmp), "0");
	else if(strncmp(value, "Auto", 4) == 0)
		res = set_nvram(WLAN_COMMON_NODE, strcat_r(prefix, "HT_BW", tmp), "1");
	else if(strncmp(value, "40MHz", 5) == 0)
		res = set_nvram(WLAN_COMMON_NODE, strcat_r(prefix, "HT_BW", tmp), "2");
#endif

	return res;
}

static int get_wifi_radio_extensionchannel(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *exchannel = NULL;
#else 	/* DSL_ASUSWRT*/
	int exchannel;
#endif
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
#ifndef RTCONFIG_BCMWL6
	exchannel = nvram_safe_get(strcat_r(prefix, "nctrlsb", tmp));
	
	if(strcmp(nvram_safe_get(strcat_r(prefix, "bw", tmp)), "1") == 0 || strcmp(nvram_safe_get(strcat_r(prefix, "bw", tmp)), "2") == 0)
	{
		if(strncmp(exchannel, "lower", 5) == 0)
			snprintf(buf, sizeof(buf), "%s", "AboveControlChannel");
		else if(strncmp(exchannel, "upper", 5) == 0)
			snprintf(buf, sizeof(buf), "%s", "BelowControlChannel");
		else
			snprintf(buf, sizeof(buf), "%s", "Auto");
	}
	else{
		snprintf(buf, sizeof(buf), "%s", "");
	}
#else
	exchannel = nvram_safe_get(strcat_r(prefix, "chanspec", tmp));
	
	if(strcmp(nvram_safe_get(strcat_r(prefix, "bw", tmp)), "0") == 0 || strcmp(nvram_safe_get(strcat_r(prefix, "bw", tmp)), "2") == 0)
	{
		if(strchr(exchannel, 'l') != NULL)
			snprintf(buf, sizeof(buf), "%s", "AboveControlChannel");
		else if(strchr(exchannel, 'u') != NULL)
			snprintf(buf, sizeof(buf), "%s", "BelowControlChannel");
		else
			snprintf(buf, sizeof(buf), "%s", "Auto");	
	}
	else
		snprintf(buf, sizeof(buf), "%s", "");
#endif

#else 	/* DSL_ASUSWRT */
	exchannel = tcapi_get_int(WLAN_COMMON_NODE, strcat_r(prefix, "HT_EXTCHA", tmp));
	if(tcapi_get_int(WLAN_COMMON_NODE, strcat_r(prefix, "HT_BW", tmp)) == 1 || 
		tcapi_get_int(WLAN_COMMON_NODE, strcat_r(prefix, "HT_BW", tmp)) == 2)
	{
		if (tcapi_get_int(WLAN_COMMON_NODE, strcat_r(prefix, "Channel", tmp))) {
			if(exchannel == 0)	/* lower */
				snprintf(buf, sizeof(buf), "%s", "AboveControlChannel");
			else if(exchannel == 1)	/* upper */
				snprintf(buf, sizeof(buf), "%s", "BelowControlChannel");
			else
				snprintf(buf, sizeof(buf), "%s", "Auto");
		}
		else
			snprintf(buf, sizeof(buf), "%s", "");
	}
	else{
		snprintf(buf, sizeof(buf), "%s", "");
	}
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_wifi_radio_extensionchannel(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int res = -1;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
#ifndef RTCONFIG_BCMWL6
	if(nvram_safe_get(strcat_r(prefix, "bw", tmp)) != 0 && nvram_get_int(strcat_r(prefix, "channel", tmp)) >= 5 && nvram_get_int(strcat_r(prefix, "channel", tmp)) <= 10) {

		int is_high_power = nvram_get("AUTO_CHANNEL") ? 1 : 0; // for high power model
		if(is_high_power && (nvram_get_int(strcat_r(prefix, "channel", tmp)) == 5 || nvram_get_int(strcat_r(prefix, "channel", tmp)) == 7))
			res = -1;
		else if(strncmp(value, "AboveControlChannel", 19) == 0)
			res = set_nvram(strcat_r(prefix, "nctrlsb", tmp), "lower");
		else if(strncmp(value, "BelowControlChannel", 19) == 0)
			res = set_nvram(strcat_r(prefix, "nctrlsb", tmp), "upper");
		else if(strncmp(value, "Auto", 4) == 0) {
			res = set_nvram(strcat_r(prefix, "bw", tmp), "0");
		}
	}
	else
		res = -1;
#else
	if(nvram_get_int(strcat_r(prefix, "bw", tmp)) != 1) {
		char *channel = nvram_safe_get(strcat_r(prefix, "chanspec", tmp));
		char buf[32];
		if(atoi(channel) >= 5 && atoi(channel) <= 10) {
			if(strncmp(value, "AboveControlChannel", 19) == 0) {
				snprintf(buf, sizeof(buf), "%dl", atoi(channel));
				res = set_nvram(strcat_r(prefix, "chanspec", tmp), buf);
			}
			else if(strncmp(value, "BelowControlChannel", 19) == 0) {
				snprintf(buf, sizeof(buf), "%du", atoi(channel));
				res = set_nvram(strcat_r(prefix, "chanspec", tmp), buf);
			}
			else if(strncmp(value, "Auto", 4) == 0) {
				res = set_nvram(strcat_r(prefix, "bw", tmp), "1");
				res = set_nvram(strcat_r(prefix, "chanspec", tmp), "0");
			}
		}
	}
	else
		res = -1;
#endif

#else 	/* DSL_ASUSWRT */
	if(tcapi_get_int(WLAN_COMMON_NODE, strcat_r(prefix, "HT_BW", tmp)) != 0) {
		int channel = tcapi_get_int(WLAN_COMMON_NODE, strcat_r(prefix, "Channel", tmp));
		char buf[32] = {0};
		if(channel < 5) {
			if(strncmp(value, "AboveControlChannel", 19) == 0) {	/* lower */
				res = set_nvram(WLAN_COMMON_NODE, strcat_r(prefix, "HT_EXTCHA", tmp), "0");
			}
			else
				res = -1;
		}
		else if(channel >= 5 && channel <= 10)
		{
			if(strncmp(value, "AboveControlChannel", 19) == 0) {	/* lower */
				res = set_nvram(WLAN_COMMON_NODE, strcat_r(prefix, "HT_EXTCHA", tmp), "0");
			}
			else if(strncmp(value, "BelowControlChannel", 19) == 0) {	/* upper */
				res = set_nvram(WLAN_COMMON_NODE, strcat_r(prefix, "HT_EXTCHA", tmp), buf);
			}
			else if(strncmp(value, "Auto", 4) == 0) {
				res = set_nvram(WLAN_COMMON_NODE, strcat_r(prefix, "HT_BW", tmp), "0");
			}
			else
				res = -1;
		}
		else if (channel > 10)
		{
			if(strncmp(value, "BelowControlChannel", 19) == 0) {	/* upper */
				res = set_nvram(WLAN_COMMON_NODE, strcat_r(prefix, "HT_EXTCHA", tmp), buf);
			}
			else
				res = -1;
		}
	}
	else
		res = -1;
#endif

	return res;
}

static int get_wifi_radio_bssid(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char hwaddr[32] = {0};
	unsigned char ea[6] = {0};
	int subunit = 0;
#endif

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "hwaddr", tmp)));
#else 	/* DSL_ASUSWRT */
	if (!strncmp(prefix, "wl0", 3))
		tcapi_get(WLAN_COMMON_NODE, "wl0_MacAddress", hwaddr);
	else if (!strncmp(prefix, "wl1", 3))
		tcapi_get(WLAN_COMMON_NODE, "wl1_MacAddress", hwaddr);
	else
		return -1;

	subunit = (prefix[3] == '.') ? atoi(prefix + 4) : -1;

	if (subunit > 0) {
		ether_atoe(hwaddr, ea);
		snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", 
									ea[0], ea[1], ea[2], ea[3], ea[4], ea[5] + subunit);
	}
	else
		snprintf(buf, sizeof(buf), "%s", hwaddr);
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_wifi_radio_ssid(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char ssid[64] = {0};
#endif

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "ssid", tmp)));
#else 	/* DSL_ASUSWRT */
	snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WLAN_NODE, strcat_r(prefix, "ssid", tmp), ssid));
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_wifi_radio_ssid(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int res = 0;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	res = set_nvram(strcat_r(prefix, "ssid", tmp), value);
#else 	/* DSL_ASUSWRT */
	res = set_nvram(WLAN_NODE, strcat_r(prefix, "ssid", tmp), value);
#endif

	return res;
}

static int get_wifi_ap_ssidadvertisementenabled(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%s", (atoi(nvram_safe_get(strcat_r(prefix, "closed", tmp))) == 0) ? "true" : "false");
#else 	/* DSL_ASUSWRT */
	snprintf(buf, sizeof(buf), "%s", tcapi_get_int(WLAN_NODE, strcat_r(prefix, "HideSSID", tmp)) == 0 ? "true" : "false");
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_wifi_ap_ssidadvertisementenabled(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int res = 0;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

	res = string2boolean(value);
	if (res == BOOLEAN_ERROR)
		return -1;

#ifdef ASUSWRT
	return set_nvram(strcat_r(prefix, "closed", tmp), (res == BOOLEAN_TRUE) ? "0" : "1");
#else
	return set_nvram(WLAN_NODE, strcat_r(prefix, "HideSSID", tmp), (res == BOOLEAN_TRUE) ? "0" : "1");
#endif
}

static int get_wifi_ap_associatednum(node_t node, char *arg, char **value)
{
	char buf[4] = {0};
	char *path = lib_node2path(node);

	snprintf(buf, sizeof(buf), "%d", get_wireless_totalassociations(path));

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_wifi_ap_modeenabled(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);
#ifdef ASUSWRT
	char *mode_enabled = NULL;
#else 	/* DSL_ASUSWRT */
	char mode_enabled[16] = {0};
#endif
	int wep_mode = 0;

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	mode_enabled = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));
	wep_mode = atoi(nvram_safe_get(strcat_r(prefix, "wep_x", tmp)));

 	if(strncmp(mode_enabled, "open", 4) == 0 || strncmp(mode_enabled, "shared", 6) == 0){
		if(wep_mode == 0)
			snprintf(buf, sizeof(buf), "%s", "None");
		else if(wep_mode == 1)
			snprintf(buf, sizeof(buf), "%s", "WEP-64");
		else
			snprintf(buf, sizeof(buf), "%s", "WEP-128");
	}
	else{
		if(strcmp(mode_enabled, "psk") == 0)
			snprintf(buf, sizeof(buf), "%s", "WPA-Personal");
		else if(strcmp(mode_enabled, "psk2") == 0)
			snprintf(buf, sizeof(buf), "%s", "WPA2-Personal");
		else if(strcmp(mode_enabled, "pskpsk2") == 0)
			snprintf(buf, sizeof(buf), "%s", "WPA-WPA2-Personal");
		else if(strcmp(mode_enabled, "wpa") == 0)
			snprintf(buf, sizeof(buf), "%s", "WPA-Enterprise");
		else if(strcmp(mode_enabled, "wpa2") == 0)
			snprintf(buf, sizeof(buf), "%s", "WPA2-Enterprise");
		else if(strcmp(mode_enabled, "wpawpa2") == 0)
			snprintf(buf, sizeof(buf), "%s", "WPA-WPA2-Enterprise");
	}

#else 	/* DSL_ASUSWRT */
	tcapi_get(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), mode_enabled);
	wep_mode = tcapi_get_int(WLAN_NODE, strcat_r(prefix, "wep_x", tmp));

 	if(strncmp(mode_enabled, "OPEN", 4) == 0 || strncmp(mode_enabled, "SHARED", 6) == 0){
		if(wep_mode == 0)
			snprintf(buf, sizeof(buf), "%s", "None");
		else if(wep_mode == 1)
			snprintf(buf, sizeof(buf), "%s", "WEP-64");
		else
			snprintf(buf, sizeof(buf), "%s", "WEP-128");
	}
	else{
		if(strcmp(mode_enabled, "WPA2PSK") == 0)
			snprintf(buf, sizeof(buf), "%s", "WPA2-Personal");
		else if(strcmp(mode_enabled, "WPAPSKWPA2PSK") == 0)
			snprintf(buf, sizeof(buf), "%s", "WPA-WPA2-Personal");
		else if(strcmp(mode_enabled, "WPA2") == 0)
			snprintf(buf, sizeof(buf), "%s", "WPA2-Enterprise");
		else if(strcmp(mode_enabled, "WPA1WPA2") == 0)
			snprintf(buf, sizeof(buf), "%s", "WPA-WPA2-Enterprise");
		else
			return -1;
	}
#endif
	
	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_wifi_ap_modeenabled(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int res = -1;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	if(strcmp(value, "WPA-Personal") == 0){
		if(strncmp(prefix, "wl0", 3) == 0)
			res = set_nvram("wl0_nmode_x", "2");
		else
			res = set_nvram("wl1_nmode_x", "2");
		res = set_nvram(strcat_r(prefix, "auth_mode_x", tmp), "psk");
	}
	else if(strcmp(value, "WPA2-Personal") == 0)
		res = set_nvram(strcat_r(prefix, "auth_mode_x", tmp), "psk2");
	else if(strcmp(value, "WPA-WPA2-Personal") == 0)
		res = set_nvram(strcat_r(prefix, "auth_mode_x", tmp), "pskpsk2");
	else if(strcmp(value, "WPA-Enterprise") == 0){
		if(strncmp(prefix, "wl0", 3) == 0)
			res = set_nvram("wl0_nmode_x", "2");
		else
			res = set_nvram("wl1_nmode_x", "2");
		res = set_nvram(strcat_r(prefix, "auth_mode_x", tmp), "wpa");
	}
	else if(strcmp(value, "WPA2-Enterprise") == 0)
		res = set_nvram(strcat_r(prefix, "auth_mode_x", tmp), "wpa2");
	else if(strcmp(value, "WPA-WPA2-Enterprise") == 0)
		res = set_nvram(strcat_r(prefix, "auth_mode_x", tmp), "wpawpa2");
	else if(strcmp(value, "None") == 0){
		if(strncmp(prefix, "wl0", 3) == 0)
			res = set_nvram("wl0_nmode_x", "2");
		else
			res = set_nvram("wl1_nmode_x", "2");

		res = set_nvram(strcat_r(prefix, "auth_mode_x", tmp), "open");
		res = set_nvram(strcat_r(prefix, "wep_x", tmp), "0");
	}
	else if(strcmp(value, "WEP-64") == 0){
		if(strncmp(prefix, "wl0", 3) == 0)
			res = set_nvram("wl0_nmode_x", "2");
		else
			res = set_nvram("wl1_nmode_x", "2");

		res = set_nvram(strcat_r(prefix, "auth_mode_x", tmp), "shared");
		res = set_nvram(strcat_r(prefix, "wep_x", tmp), "1");
	}
	else if(strcmp(value, "WEP-128") == 0){
		if(strncmp(prefix, "wl0", 3) == 0)
			res = set_nvram("wl0_nmode_x", "2");
		else
			res = set_nvram("wl1_nmode_x", "2");

		res = set_nvram(strcat_r(prefix, "auth_mode_x", tmp), "shared");
		res = set_nvram(strcat_r(prefix, "wep_x", tmp), "2");
	}

#else 	/* DSL_ASUSWRT */
	if(strcmp(value, "WPA-Personal") == 0) {
		if(strncmp(prefix, "wl0", 3) == 0) {	/* 2.4G */
			snprintf(tmp, sizeof(tmp), "wl0_WirelessMode");
			res = set_nvram(WLAN_COMMON_NODE, tmp, "0");
		}
		else {	/* 5G */
			snprintf(tmp, sizeof(tmp), "wl1_WirelessMode");
			res = set_nvram(WLAN_COMMON_NODE, tmp, "2");
		}
		res = set_nvram(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), "WPAPSK");
	}
	else if(strcmp(value, "WPA2-Personal") == 0)
		res = set_nvram(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), "WPA2PSK");
	else if(strcmp(value, "WPA-WPA2-Personal") == 0)
		res = set_nvram(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), "WPAPSKWPA2PSK");
	else if(strcmp(value, "WPA-Enterprise") == 0) {
		if(strncmp(prefix, "wl0", 3) == 0) {	/* 2.4G */
			snprintf(tmp, sizeof(tmp), "wl0_WirelessMode");
			res = set_nvram(WLAN_COMMON_NODE, tmp, "0");
		}
		else {	/* 5G */
			snprintf(tmp, sizeof(tmp), "wl1_WirelessMode");
			res = set_nvram(WLAN_COMMON_NODE, tmp, "2");
		}
		res = set_nvram(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), "WPA");
	}
	else if(strcmp(value, "WPA2-Enterprise") == 0)
		res = set_nvram(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), "WPA2");
	else if(strcmp(value, "WPA-WPA2-Enterprise") == 0)
		res = set_nvram(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), "WPA1WPA2");
	else if (strcmp(value, "None") == 0) {
		if(strncmp(prefix, "wl0", 3) == 0) {	/* 2.4G */
			snprintf(tmp, sizeof(tmp), "wl0_WirelessMode");
			res = set_nvram(WLAN_COMMON_NODE, tmp, "0");
		}
		else {	/* 5G */
			snprintf(tmp, sizeof(tmp), "wl1_WirelessMode");
			res = set_nvram(WLAN_COMMON_NODE, tmp, "2");
		}

		res = set_nvram(WLAN_NODE, strcat_r(prefix, "wep_x", tmp), "0");

		if (res == 0)
			res = set_nvram(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), "OPEN");
	}
	else if(strcmp(value, "WEP-64") == 0) {
		if(strncmp(prefix, "wl0", 3) == 0) {	/* 2.4G */
			snprintf(tmp, sizeof(tmp), "wl0_WirelessMode");
			res = set_nvram(WLAN_COMMON_NODE, tmp, "0");
		}
		else {	/* 5G */
			snprintf(tmp, sizeof(tmp), "wl1_WirelessMode");
			res = set_nvram(WLAN_COMMON_NODE, tmp, "2");
		}

		res = set_nvram(WLAN_NODE, strcat_r(prefix, "wep_x", tmp), "1");

		if (res == 0)
			res = set_nvram(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), "SHARED");
	}
	else if(strcmp(value, "WEP-128") == 0) {
		if(strncmp(prefix, "wl0", 3) == 0) {	/* 2.4G */
			snprintf(tmp, sizeof(tmp), "wl0_WirelessMode");
			res = set_nvram(WLAN_COMMON_NODE, tmp, "0");
		}
		else {	/* 5G */
			snprintf(tmp, sizeof(tmp), "wl1_WirelessMode");
			res = set_nvram(WLAN_COMMON_NODE, tmp, "2");
		}

		res = set_nvram(WLAN_NODE, strcat_r(prefix, "wep_x", tmp), "2");

		if (res == 0)
			res = set_nvram(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), "SHARED");
	}
#endif

	return res;
}

static int get_wifi_ap_wepkey(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);
#ifdef ASUSWRT
	char *auth_mode = NULL;
#else 	/* DSL_ASUSWRT */
	char auth_mode[16] = {0};
#endif

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	auth_mode = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));
	
	if (strcmp(auth_mode, "open") == 0 || strcmp(auth_mode, "shared") == 0) {
		char *wep = nvram_safe_get(strcat_r(prefix, "wep_x", tmp));

		if (strcmp(wep, "1") == 0 || strcmp(wep, "2") == 0) {
			char key[8] = {0};
			
			snprintf(key, sizeof(key), "key%s", nvram_safe_get(strcat_r(prefix, "key", tmp)));
			snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, key, tmp)));
		}
	}
#else 	/* DSL_ASUSWRT */
	tcapi_get(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), auth_mode);

	if (strcmp(auth_mode, "OPEN") == 0 || strcmp(auth_mode, "SHARED") == 0) {
		int wep = tcapi_get_int(WLAN_NODE, strcat_r(prefix, "wep_x", tmp));

		if (wep == 1 || wep == 2) {
			char key[8] = {0}, wep_key[64] = {0};
			
			snprintf(key, sizeof(key), "key%d", tcapi_get_int(WLAN_NODE, strcat_r(prefix, "key", tmp)));
			snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WLAN_NODE, strcat_r(prefix, key, tmp), wep_key));
		}
	}	
#endif
	
	*value = strdup(buf);
	return *value ? 0 : -1;	
}

static int set_wifi_ap_wepkey(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int res = -1;
	char *path = lib_node2path(node);
#ifdef ASUSWRT
	char *auth_mode = NULL;
#else 	/* DSL_ASUSWRT */
	char auth_mode[16] = {0};
#endif

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	auth_mode = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));

	if (strcmp(auth_mode, "open") && strcmp(auth_mode, "shared"))
		return -1;

	if(strlen(value) == 5){
		if(strncmp(prefix, "wl0", 3) == 0)
			nvram_set("wl0_nmode_x", "2");
		else
			nvram_set("wl1_nmode_x", "2");

		if (strcmp(auth_mode, "open") && strcmp(auth_mode, "shared")) 
			nvram_set(strcat_r(prefix, "auth_mode_x", tmp), "shared");
		
		nvram_set(strcat_r(prefix, "wep_x", tmp), "1");
		nvram_set(strcat_r(prefix, "key", tmp), "1");
		res = set_nvram(strcat_r(prefix, "key1", tmp), value);
	}
	else if(strlen(value) == 13){
		if(strncmp(prefix, "wl0", 3) == 0)
			nvram_set("wl0_nmode_x", "2");
		else
			nvram_set("wl1_nmode_x", "2");

		if (strcmp(auth_mode, "open") && strcmp(auth_mode, "shared")) 
			nvram_set(strcat_r(prefix, "auth_mode_x", tmp), "shared");

		nvram_set(strcat_r(prefix, "wep_x", tmp), "2");
		nvram_set(strcat_r(prefix, "key", tmp), "1");
		res = set_nvram(strcat_r(prefix, "key1", tmp), value);
	}
#else 	/* DSL_ASUSWRT */
	tcapi_get(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), auth_mode);

	if (strcmp(auth_mode, "OPEN") && strcmp(auth_mode, "SHARED"))
		return -1;
	if(strlen(value) == 5) {
		if(strncmp(prefix, "wl0", 3) == 0) {	/* 2.4G */
			snprintf(tmp, sizeof(tmp), "wl0_WirelessMode");
			res = set_nvram(WLAN_COMMON_NODE, tmp, "0");
		}
		else {	/* 5G */
			snprintf(tmp, sizeof(tmp), "wl1_WirelessMode");
			res = set_nvram(WLAN_COMMON_NODE, tmp, "2");
		}

		set_nvram(WLAN_NODE, strcat_r(prefix, "wep_x", tmp), "1");
		set_nvram(WLAN_NODE, strcat_r(prefix, "key", tmp), "1");
		res = set_nvram(WLAN_NODE, strcat_r(prefix, "key1", tmp), value);
	}
	else if(strlen(value) == 13) {
		if(strncmp(prefix, "wl0", 3) == 0) {	/* 2.4G */
			snprintf(tmp, sizeof(tmp), "wl0_WirelessMode");
			res = set_nvram(WLAN_COMMON_NODE, tmp, "0");
		}
		else {	/* 5G */
			snprintf(tmp, sizeof(tmp), "wl1_WirelessMode");
			res = set_nvram(WLAN_COMMON_NODE, tmp, "2");
		}

		set_nvram(WLAN_NODE, strcat_r(prefix, "wep_x", tmp), "2");
		set_nvram(WLAN_NODE, strcat_r(prefix, "key", tmp), "1");
		res = set_nvram(WLAN_NODE, strcat_r(prefix, "key1", tmp), value);
	}	
#endif

	return res;
}

static int get_wifi_ap_presharedkey(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);
#ifdef ASUSWRT	
	char *auth_mode = NULL;
#else 	/* DSL_ASUSWRT */
	char auth_mode[16] = {0}, wpa_psk[64] = {0};
#endif

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	auth_mode = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));

	if (strncmp(auth_mode, "psk", 3) == 0)
		snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp)));
#else 	/* DSL_ASUSWRT */
	tcapi_get(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), auth_mode);
	
	if (strcmp(auth_mode, "WPA2PSK") == 0 || strcmp(auth_mode, "WPAPSKWPA2PSK") == 0)
		snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WLAN_NODE, strcat_r(prefix, "wpa_psk", tmp), wpa_psk));
#endif
	
	*value = strdup(buf);
	return *value ? 0 : -1;	
}

static int set_wifi_ap_presharedkey(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int res = -1;
	char *path = lib_node2path(node);
#ifdef ASUSWRT	
	char *auth_mode = NULL;
#else 	/* DSL_ASUSWRT */
	char auth_mode[16] = {0};
#endif

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	auth_mode = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));

	if (strncmp(auth_mode, "psk", 3) == 0)
		res = set_nvram(strcat_r(prefix, "wpa_psk", tmp), value);
#else 	/* DSL_ASUSWRT */
	tcapi_get(WLAN_NODE, strcat_r(prefix, "auth_mode_x", tmp), auth_mode);
	
	if (strcmp(auth_mode, "WPA2PSK") == 0 || strcmp(auth_mode, "WPAPSKWPA2PSK") == 0)
		res = set_nvram(WLAN_NODE, strcat_r(prefix, "wpa_psk", tmp), value);
#endif

	return res;
}

static int get_wifi_ap_rekeyinginterval(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	if (strncmp(prefix, "wl0", 3) == 0)
		snprintf(buf, sizeof(buf), "%s", nvram_safe_get("wl0_wpa_gtk_rekey"));
	else if (strncmp(prefix, "wl1", 3) == 0)
		snprintf(buf, sizeof(buf), "%s", nvram_safe_get("wl1_wpa_gtk_rekey"));
#else 	/* DSL_ASUSWRT */
	if (strncmp(prefix, "wl0", 3) == 0)
		snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WLAN_NODE, "wl0_RekeyInterval", tmp));
	else if (strncmp(prefix, "wl1", 3) == 0)
		snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WLAN_NODE, "wl1_RekeyInterval", tmp));
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_wifi_ap_rekeyinginterval(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int res = -1;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	if(strncmp(prefix, "wl0", 3) == 0)
		res = set_nvram("wl0_wpa_gtk_rekey", value);
	else if (strncmp(prefix, "wl1", 3) == 0)
		res = set_nvram("wl1_wpa_gtk_rekey", value);
#else 	/* DSL_ASUSWRT */
	if(strncmp(prefix, "wl0", 3) == 0)
		res = set_nvram(WLAN_NODE, "wl0_RekeyInterval", value);
	else if (strncmp(prefix, "wl1", 3) == 0)
		res = set_nvram(WLAN_NODE, "wl1_RekeyInterval", value);
#endif

	return res;
}

static int get_wifi_ap_radiusserip(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char radius_ip[64] = {0};
#endif

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "radius_ipaddr", tmp)));
#else 	/* DSL_ASUSWRT */
	snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WLAN_NODE, strcat_r(prefix, "RADIUS_Server", tmp), radius_ip));
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_wifi_ap_radiusserip(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int res = 0;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	res = set_nvram(strcat_r(prefix, "radius_ipaddr", tmp), value);
#else 	/* DSL_ASUSWRT */
	res = set_nvram(WLAN_NODE, strcat_r(prefix, "RADIUS_Server", tmp), value);
#endif

	return res;
}

static int get_wifi_ap_radiusserport(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char radius_port[8] = {0};
#endif

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "radius_port", tmp)));
#else 	/* DSL_ASUSWRT */
	snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WLAN_NODE, strcat_r(prefix, "RADIUS_Port", tmp), radius_port));
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_wifi_ap_radiusserport(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int res = 0;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	res = set_nvram(strcat_r(prefix, "radius_port", tmp), value);
#else 	/* DSL_ASUSWRT */
	res = set_nvram(WLAN_NODE, strcat_r(prefix, "RADIUS_Port", tmp), value);
#endif

	return res;
}

static int get_wifi_ap_radiussersecret(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char radius_secret[64] = {0};
#endif

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "radius_key", tmp)));
#else 	/* DSL_ASUSWRT */
	snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WLAN_NODE, strcat_r(prefix, "RADIUS_Key1", tmp), radius_secret));
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_wifi_ap_radiussersecret(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int res = 0;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	res = nvram_set(strcat_r(prefix, "radius_key", tmp), value);
#else 	/* DSL_ASUSWRT */
	res = set_nvram(WLAN_NODE, strcat_r(prefix, "RADIUS_Key1", tmp), value);
#endif

	return res;
}

static int get_wifi_ap_wps_enable(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);
#ifdef ASUSWRT
	char *wpsmode = NULL;
#endif

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	wpsmode = nvram_safe_get(strcat_r(prefix, "wps_mode", tmp));

	if(strncmp(wpsmode, "disabled", 8) == 0)
		snprintf(buf, sizeof(buf), "%s", "false");
	else
		snprintf(buf, sizeof(buf), "%s", "true");
#else 	/* DSL_ASUSWRT */
	snprintf(buf, sizeof(buf), "%s", tcapi_get_int(WLAN_COMMON_NODE, strcat_r(prefix, "wps_enable", tmp)) ? "true" : "false");
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_wifi_ap_wps_enable(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};
	int res = 0;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	if(!war_strcasecmp(value, "1") || !war_strcasecmp(value, "true"))
		res = set_nvram(strcat_r(prefix, "wps_mode", tmp), "enabled");
	else
		res = set_nvram(strcat_r(prefix, "wps_mode", tmp), "disabled");
#else 	/* DSL_ASUSWRT */
	if(!war_strcasecmp(value, "1") || !war_strcasecmp(value, "true"))
		res = set_nvram(WLAN_COMMON_NODE, strcat_r(prefix, "wps_enable", tmp), "1");
	else
		res = set_nvram(WLAN_COMMON_NODE, strcat_r(prefix, "wps_enable", tmp), "0");
#endif

	return res;
}

static int get_wifi_ap_wps_configmethodsenabled(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, buf[32] = {0}, tmp[32] = {0};
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%s", (nvram_contains_word("wps_sta_pin","00000000") ? "PushButton" : "PIN"));
#else 	/* DSL_ASUSWRT */
	if (tcapi_get_int(WLAN_NODE, strcat_r(prefix, "WPSMode", tmp)))
		snprintf(buf, sizeof(buf), "PushButton");
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_wifi_ap_wps_configmethodsenabled(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wlXXXXXXXXXX_")] = {0}, tmp[32] = {0};;
	int res = -1;
	char *path = lib_node2path(node);

	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	if(strcmp(value,"PushButton") == 0) {
		set_nvram("wps_enable_x", "1");
		set_nvram("wps_sta_pin", "00000000");

		res = set_nvram(strcat_r(prefix, "wps_mode", tmp), "enabled");
	}
	else if(strcmp(value,"PIN") == 0)
	{
		set_nvram("wps_enable_x", "1");
		res = set_nvram(strcat_r(prefix, "wps_mode", tmp), "enabled");
	}
#else 	/* DSL_ASUSWRT */
	if(strcmp(value,"PushButton") == 0) {
		if (tcapi_get_int(WLAN_NODE, strcat_r(prefix, "wps_enable", tmp))) {
			set_nvram("Info_WLan", "WPSActiveStatus", "1");
			set_nvram(WLAN_NODE, strcat_r(prefix, "WPSConfMode", tmp), "7");
			set_nvram(WLAN_NODE, strcat_r(prefix, "WPSMode", tmp), "0");
			res = set_nvram(WLAN_NODE, strcat_r(prefix, "WPSConfStatus", tmp), "2");
		}
	}
#endif

	return res;
}

static int get_ip_ipaddress(node_t node, char *arg, char **value)
{
	char buf[64] = {0}, prefix[16] = {0}, tmp[64] = {0};
#ifdef ASUSWRT
	char *wan_proto = NULL, *wan_dhcpenable = NULL;
#else 	/* DSL_ASUSWRT */
	char ipaddr[32] = {0}, pvc[32] = {0};
#endif
	char *path = lib_node2path(node);

	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	if (strstr(path, "IP") != NULL) {	/* IP */
		if (!strcmp(prefix, "lan_"))
			snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)));
		else if (!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp") ) {
			memset(buf, 0 , sizeof(buf));
			strncpy(buf, path, strlen(LEN_OF_IP_INTERFACE_X));
			strcat(buf, ".LowerLayers");
			if (strstr(__get_parameter_value(buf, tmp), "PPP")) {
				snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)));
			}
			else
			{
				wan_dhcpenable = nvram_safe_get(strcat_r(prefix, "dhcpenable_x", tmp));
				snprintf(buf, sizeof(buf), "%s", !strcmp(wan_dhcpenable, "1") ? 
								nvram_safe_get(strcat_r(prefix, "xipaddr", tmp)) : 
								nvram_safe_get(strcat_r(prefix, "ipaddr_x", tmp)));
			}
		}
		else
			snprintf(buf, sizeof(buf), "%s", !strcmp(wan_proto, "dhcp") ? 
							nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)) : 
							nvram_safe_get(strcat_r(prefix, "ipaddr_x", tmp)));
	}

#else 	/* DSL_ASUSWRT */
	if (strstr(path, "IP") != NULL) {	/* IP */
		if (!strcmp(prefix, "lan_"))
			snprintf(buf, sizeof(buf), "%s", tcapi_get_string(LAN_NODE, "IP", tmp));
		else
		{
			snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
			if (tcapi_match(pvc, "ISP", "0"))	/* dhcp */
				snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "ipaddr", tmp), ipaddr));
			else if (tcapi_match(pvc, "ISP", "1")) 	/* static */
				snprintf(buf, sizeof(buf), "%s", tcapi_get_string(pvc, "IPADDR", ipaddr));
			else if (tcapi_match(pvc, "ISP", "2")) {
				if (tcapi_match(pvc, "PPPGETIP", "Static"))
					snprintf(buf, sizeof(buf), "%s", tcapi_get_string(pvc, "IPADDR", ipaddr));
				else
					snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "ipaddr", tmp), ipaddr));
			}
			else
				return -1;
		}
	}
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_ip_ipaddress(node_t node, char *arg, char *value)
{
	char prefix[16] = {0}, tmp[64] = {0};
#ifdef ASUSWRT
	char buf[64] = {0};
	char *wan_proto = NULL, *wan_dhcpenable = NULL;
#else 	/* DSL_ASUSWRT */
	char pvc[32] = {0};
#endif
	int res = 0;
	char *path = lib_node2path(node);

	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	if (strstr(path, "IP") != NULL) {	/* IP */
		if (!strcmp(prefix, "lan_")) {
			res = set_nvram(strcat_r(prefix, "ipaddr", tmp), value);
		}
		else if (!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp")) {
			memset(buf, 0 , sizeof(buf));
			strncpy(buf, path, strlen(LEN_OF_IP_INTERFACE_X));
			strcat(buf, ".LowerLayers");
			if (strstr(__get_parameter_value(buf, tmp), "PPP"))
				return -1;
			else {
				wan_dhcpenable = nvram_safe_get(strcat_r(prefix, "dhcpenable_x", tmp));
				if(!strcmp(wan_dhcpenable, "1"))	/* can't be modified */
					return -1;
				else
					res = set_nvram(strcat_r(prefix, "ipaddr_x", tmp), value);
			}
		}
		else
		{
			if (!strcmp(wan_proto, "dhcp"))	/* can't be modified */
				return -1;
			else
				res = set_nvram(strcat_r(prefix, "ipaddr_x", tmp), value);
		}
	}
	else	/* PPP */		/* can't be modified */
		return -1;

#else 	/* DSL_ASUSWRT */
	if(strstr(path, "IP") != NULL) {	/* IP */
		snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
		if (tcapi_match(pvc, "ISP", "1")) 	/* static */
			res = set_nvram(pvc, "IPADDR", value);
		else
			return -1;	/* can't be modified */
	}	
	else	/* PPP */		/* can't be modified */
		return -1;
#endif

	return res;
}

static int set_ip_subnetmask(node_t node, char *arg, char *value)
{
	char prefix[16] = {0}, tmp[64] = {0};
#ifdef ASUSWRT
	char buf[64] = {0};
	char *wan_proto = NULL, *wan_dhcpenable = NULL;
#else 	/* DSL_ASUSWRT */
	char pvc[32] = {0};
#endif
	int res = 0;
	char *path = lib_node2path(node);

	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	if (strstr(path, "IP") != NULL) {	/* IP */
		if (!strcmp(prefix, "lan_")) {
			res = set_nvram(strcat_r(prefix, "netmask", tmp), value);
		}
		else if (!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp")) {
			memset(buf, 0 , sizeof(buf));
			strncpy(buf, path, strlen(LEN_OF_IP_INTERFACE_X));
			strcat(buf, ".LowerLayers");
			if (strstr(__get_parameter_value(buf, tmp),"PPP"))
				return -1;
			else {
				wan_dhcpenable = nvram_safe_get(strcat_r(prefix, "dhcpenable_x", tmp));
				if (!strcmp(wan_dhcpenable, "1"))	/* can't be modified */
					return -1;
				else
					res = set_nvram(strcat_r(prefix, "netmask_x", tmp), value);
			}
		}
		else
		{
			if (!strcmp(wan_proto, "dhcp"))	/* can't be modified */
				return -1;
			else
				res = set_nvram(strcat_r(prefix, "netmask_x", tmp), value);
		}
	}

#else 	/* DSL_ASUSWRT */
	if(strstr(path, "IP") != NULL) {	/* IP */
		snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
		if (tcapi_match(pvc, "ISP", "1")) 	/* static */
			res = set_nvram(pvc, "NETMASK", value);
		else
			return -1;	/* can't be modified */
	}	
	else	/* PPP */		/* can't be modified */
		return -1;
#endif

	return res;
}

static int get_ip_subnetmask(node_t node, char *arg, char **value)
{
	char buf[64] = {0}, prefix[16] = {0}, tmp[64] = {0};
#ifdef ASUSWRT
	char *wan_proto = NULL, *wan_dhcpenable = NULL;
#else 	/* DSL_ASUSWRT */
	char netmask[32] = {0}, pvc[32] = {0};
#endif
	char *path = lib_node2path(node);

	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));

#ifdef ASUSWRT
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	
	if (strstr(path, "IP") != NULL) {	/* IP */
		if (!strcmp(prefix, "lan_"))
			snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "netmask", tmp)));
		else if (!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp") ) {
			memset(buf, 0 , sizeof(buf));
			strncpy(buf, path, strlen(LEN_OF_IP_INTERFACE_X));
			strcat(buf, ".LowerLayers");
			if (strstr(__get_parameter_value(buf, tmp), "PPP"))
				snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "netmask", tmp)));
			else {
				wan_dhcpenable = nvram_safe_get(strcat_r(prefix, "dhcpenable_x", tmp));
				snprintf(buf, sizeof(buf), "%s", !strcmp(wan_dhcpenable, "1") ? 
								nvram_safe_get(strcat_r(prefix, "xnetmask", tmp)) : 
								nvram_safe_get(strcat_r(prefix, "netmask_x", tmp)));
			}
		}
		else
			snprintf(buf, sizeof(buf), "%s", !strcmp(wan_proto, "dhcp") ? 
							nvram_safe_get(strcat_r(prefix, "netmask", tmp)) : 
							nvram_safe_get(strcat_r(prefix, "netmask_x", tmp)));
	}

#else 	/* DSL_ASUSWRT */
	if (strstr(path, "IP") != NULL) {	/* IP */
		if (!strcmp(prefix, "lan_"))
			snprintf(buf, sizeof(buf), "%s", tcapi_get_string(LAN_NODE, "netmask", tmp));
		else
		{
			snprintf(pvc, sizeof(pvc), "%s", convert_prefix_to_pvc(prefix, tmp));
			if (tcapi_match(pvc, "ISP", "0"))	/* dhcp */
				snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "netmask", tmp), netmask));
			else if (tcapi_match(pvc, "ISP", "1")) 	/* static */
				snprintf(buf, sizeof(buf), "%s", tcapi_get_string(pvc, "NETMASK", netmask));
			else if (tcapi_match(pvc, "ISP", "2")) {
				if (tcapi_match(pvc, "PPPGETIP", "Static"))
					snprintf(buf, sizeof(buf), "%s", tcapi_get_string(pvc, "NETMASK", netmask));
				else
					snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "netmask", tmp), netmask));
			}
			else
				return -1;
		}
	}
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_upnpenable(node_t node, char *arg, char **value)
{
	char buf[32] = {0};
#ifdef ASUSWRT	
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0}, tmp[32] = {0};
#ifdef RTCONFIG_DUALWAN
	int unit = wan_primary_ifunit();
#else
	int unit = 0;
#endif
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	snprintf(buf, sizeof(buf), "%s", (atoi(nvram_safe_get(strcat_r(prefix, "upnp_enable", tmp))) == 1) ? "true" : "false");

#else 	/* DSL_ASUSWRT */
	snprintf(buf, sizeof(buf), "%s", tcapi_match(UPNPD_NODE, "Active", "Yes") ? "true" : "false");
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_upnpenable(node_t node, char *arg, char *value)
{
#ifdef ASUSWRT
	char buf[32] = {0}, prefix[sizeof("wanXXXXXXXXXX_")] = {0};
#endif
	int res = 0;
	char *value_conv = NULL;

	value_conv = (!war_strcasecmp(value, "1") || !war_strcasecmp(value, "true")) ? "1" : "0";

#ifdef ASUSWRT
#ifdef RTCONFIG_DUALWAN
	int unit = wan_primary_ifunit();
#else
	int unit = 0;
#endif
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	sprintf(buf, "%supnp_enable", prefix);
	res = set_nvram(buf, value_conv);

#else 	/* DSL_ASUSWRT */
	res = set_nvram(UPNPD_NODE, "Active", !strcmp(value_conv, "1") ? "Yes" : "No");
#endif

	return res;
}

static int get_eth_name(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")] = {0}, tmp[32] = {0}, buf[32] = {0};
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char ifname[32] = {0};
#endif

	snprintf(prefix, sizeof(prefix), "%s", ethernet_prefix_by_path(path, tmp));

	if (strlen(prefix) == 0) {
		*value = strdup("");
		return 0;
	}

	if (strstr(path, "PPP") != NULL)
#ifdef ASUSWRT
		snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "pppoe_ifname", tmp)));
#else 	/* DSL_ASUSWRT */
		snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "pppoe_ifname", tmp), ifname));
#endif
	else
	{
		if (!strncmp(prefix, "lan", 3) && strstr(path, ETH_IF))
			snprintf(buf, sizeof(buf), "%s", DEF_ETH_LAN_IFNAME);
		else
#ifdef ASUSWRT
			snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "ifname", tmp)));
#else 	/* DSL_ASUSWRT */
			snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "ifname", tmp), ifname));
#endif
	}

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_eth_macaddress(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, prefix[sizeof("wanXXXXXXXXXX_")] = {0}, tmp[32] = {0}, eabuf[32] = {0};
	char *p = NULL;;
	struct ifreq ifr;
	int sfd;
	char *path = lib_node2path(node);
#ifdef DSL_ASUSWRT
	char ifname[32] = {0};
#endif

	if ((sfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) 
		return -1;
	
	snprintf(prefix, sizeof(prefix), "%s", ethernet_prefix_by_path(path, tmp));

	if (strlen(prefix) == 0) {
		*value = strdup("");
		return 0;
	}

	if(strstr(path, "PPP") != NULL)
#ifdef ASUSWRT
		snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "pppoe_ifname", tmp)));
#else 	/* DSL_ASUSWRT */
		snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "pppoe_ifname", tmp), ifname));
#endif
	else
	{
		if (!strncmp(prefix, "lan", 3) && strstr(path, ETH_IF))
			snprintf(buf, sizeof(buf), "%s", DEF_ETH_LAN_IFNAME);
		else
#ifdef ASUSWRT		
			snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "ifname", tmp)));
#else 	/* DSL_ASUSWRT */
			snprintf(buf, sizeof(buf), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, "ifname", tmp), ifname));
#endif
	}

	strlcpy(ifr.ifr_name, buf, IFNAMSIZ);
	if (ioctl(sfd, SIOCGIFHWADDR, &ifr) == 0)
	{
		p = ether_etoa((unsigned char *)ifr.ifr_hwaddr.sa_data, eabuf);
		*value = strdup(p);
	}
	close(sfd);
	return *value ? 0 : -1;
}


#if defined(RTCONFIG_IPV6) || defined(TCSUPPORT_IPV6)
static int get_ipv6rd_enable(node_t node, char *arg, char **value)
{
	char buf[16] = {0};

#ifdef ASUSWRT
	snprintf(buf, sizeof(buf), "%s", (strcmp(nvram_safe_get("ipv6_service"), "6rd") == 0) ? "true" : "false");
#else 	/* DSL_ASUSWRT */
	snprintf(buf, sizeof(buf), "%s", tcapi_match(IPV6RD_NODE, "Active", "Yes") ? "true" : "false");
#endif

	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_ipv6rd_enable(node_t node, char *arg, char *value)
{
	int res = 0;

#ifdef ASUSWRT
	if(!strcasecmp(value, "true") || !strcasecmp(value, "1"))
		res = set_nvram("ipv6_service", "6rd");
	else
		res = set_nvram("ipv6_radvd", "disabled");

#else 	/* DSL_ASUSWRT */
	char *value_conv = NULL;

	value_conv = (!war_strcasecmp(value, "1") || !war_strcasecmp(value, "true")) ? "1" : "0";
	res = set_nvram(IPV6RD_NODE, "Active", !strcmp(value_conv, "1") ? "Yes" : "No");
#endif

	return res;
}
#endif	/* #if defined(RTCONFIG_IPV6) || defined(TCSUPPORT_IPV6) */

#ifdef RTCONFIG_IPV6
static int get_ipv6_enable(node_t node, char *arg, char **value)
{
	if (!(ipv6_enabled() && is_routing_enabled()))
		*value = "false";
	else
		*value = "true";
	*value = strdup(*value);
	return *value ? 0 : -1;
}

static int set_ipv6_enable(node_t node, char *arg, char *value)
{
	int res = 0;
	if (!(ipv6_enabled() && is_routing_enabled()))
		res = nvram_set("ipv6_radvd", "disabled");
	else
		res = nvram_set("ipv6_service", value);
	
	return res;
}

static int get_ipv6_address(node_t node, char *arg, char **value)
{
	char buf[32] = {0};
	char *path = lib_node2path(node);
	int unit = ethernet_unit_by_path(path);

	if (unit == -1)	/* something wrong */
		return -1;
	else if (unit == 2)
		snprintf(buf, sizeof(buf), "%s", nvram_safe_get("ipv6_rtr_addr"));
	else
		snprintf(buf, sizeof(buf), "%s", nvram_safe_get("ipv6_ipaddr"));

	*value = strdup(buf);	
	return *value ? 0 : -1;
}

static int set_ipv6_address(node_t node, char *arg, char *value)
{
	int res = 0;
	char *path = lib_node2path(node);
	int unit  = ethernet_unit_by_path(path);

	if (unit == -1)	/* something wrong */
		return -1;
	else if (unit == 2)
		res = nvram_set("ipv6_rtr_addr", value);
	else
		res = nvram_set("ipv6_ipaddr", value);

	return res;
}
#endif


void update_gateway_info()
{
	char vivso[128] = {0}, buf[128] = {0};	

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	//vivso = nvram_safe_get("vivso");
#ifdef ASUSWRT
	tr_log(LOG_DEBUG, "vivso: %s", nvram_safe_get("vivso"));
	snprintf(vivso, sizeof(vivso), "%s", nvram_safe_get("vivso"));
#else 	/* DSL_ASUSWRT */
	tr_log(LOG_DEBUG, "vivso: %s", tcapi_get_string(TR069_NODE, "vivso", buf));
	tcapi_get(TR069_NODE, "vivso", vivso);
#endif
	
	if(strlen(vivso)) {	/* update gateway info and set inform parameter */
		char *oui = NULL, *serial = NULL, *class = NULL;
		if ((vstrsep(vivso, ",", &oui, &serial, &class) != 3))
			return;

		tr_log(LOG_DEBUG, "oui: %s, serial: %s, clas: %s", oui, serial, class);
		snprintf(buf, sizeof(buf), "%s.ManufacturerOUI", DEVICE_GATEWAYINFO);
		__set_parameter_value(buf, oui);
		add_inform_parameter(buf, 0);

		snprintf(buf, sizeof(buf), "%s.SerialNumber", DEVICE_GATEWAYINFO);
		__set_parameter_value(buf, serial);
		add_inform_parameter(buf, 0);

		snprintf(buf, sizeof(buf), "%s.ProductClass", DEVICE_GATEWAYINFO);
		__set_parameter_value(buf, class);
		add_inform_parameter(buf, 0);
	}

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);
}

int update_host()
{
	FILE *fp;
	char *next;
#ifdef ASUSWRT
	char *hwaddr, *ipaddr, *name;
	unsigned int expires;
	char val[32] = {0}, tmp[64] = {0};
	char *macaddr, *ip6addr, *hostname, *nextline;
#else 	/* DSL_ASUSWRT */
	char mac[17] = {0}, ipaddr[16] = {0}, expire[10] = {0}, name[32] = {0};
#endif
	char line[256] = {0}, buf[128] = {0};
	int ret = 0, i = 0;

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

#ifdef ASUSWRT
	if (!(fp = fopen("/var/lib/misc/dnsmasq.leases", "r")))
#else 	/* DSL_ASUSWRT */
	if (!(fp = fopen("/etc/udhcp_lease", "r")))
#endif
		return ret;

	if(delete_all_instance(HOSTS_HOST)) {
		tr_log(LOG_ERROR, "delete_all_instance failed: %s", HOSTS_HOST);
		return -1;
    }

	while ((next = fgets(line, sizeof(line), fp)) != NULL) {
#ifdef ASUSWRT
		/* line should start from numeric value */
		if (sscanf(next, "%u ", &expires) != 1)
			continue;

		strsep(&next, " ");	/* passs lease time */
		hwaddr = strsep(&next, " ") ? : "";
		ipaddr = strsep(&next, " ") ? : "";
		name = strsep(&next, " ") ? : "";
#else 	/* DSL_ASUSWRT */
		memset(mac, 0, sizeof(mac));
		memset(ipaddr, 0, sizeof(ipaddr));
		memset(expire, 0, sizeof(expire));
		memset(name, 0, sizeof(name));
		sscanf(line, "%s %s %s %s",mac, ipaddr, expire, name);		
#endif
		i++;

		sprintf(buf, "%s.", HOSTS_HOST);
		if(add_object(buf, strlen(buf)) >= 9000) {
			tr_log(LOG_ERROR, "add_object failed: %s", buf);
			return -1;				
		}
		sprintf(buf, "%s.%d.IPAddress", HOSTS_HOST, i);
		__set_parameter_value(buf, ipaddr);
		sprintf(buf, "%s.%d.HostName", HOSTS_HOST, i);
		__set_parameter_value(buf, name);

#ifdef RTCONFIG_IPV6
		/* ipv6 is enable */
		int k = 0;
		if (ipv6_enabled() && is_routing_enabled()) {
			FILE *stream;
			if (!(stream = fopen("/tmp/ipv6_client_info", "r")))
				return ret;

			sprintf(buf, "%s.%d.IPv6Address", HOSTS_HOST, i);
			if(delete_all_instance(buf)) {
				tr_log(LOG_ERROR, "delete_all_instance failed: %s", HOSTS_HOST);
				return -1;
			}
			while ((nextline = fgets(line, sizeof(line), stream)) != NULL) {
				if(strstr(nextline, hwaddr))
				{
					strsep(&nextline, " ");
					hostname = strsep(&nextline, " ") ? : "";
					macaddr = strsep(&nextline, " ") ? : "";
					ip6addr = strsep(&nextline, " ") ? : "";

					sprintf(buf, "%s.%d.IPv6Address", HOSTS_HOST, i);
					if((k = add_object(buf, strlen(buf))) >= 9000) {
						tr_log(LOG_ERROR, "add_object failed: %s", buf);
						return -1;				
					}
					sprintf(buf, "%s.%d.IPv6Address.%d.IPAddress", HOSTS_HOST, i, k);
					__set_parameter_value(buf, ipaddr);
					
					sprintf(buf, "%s.%d.IPv6AddressNumberOfEntries", HOSTS_HOST, i);
					snprintf(val, sizeof(val), "%d", atoi(__get_parameter_value(buf, tmp)) + 1);
					__set_parameter_value(buf, val);
				}
			}
			fclose(stream);
		}
		/* end */
#endif
	}
	fclose(fp);

	sprintf(buf, "%d", i);
	__set_parameter_value(HOSTS_NUM, buf);

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return ret;
}

int update_dhcpv4_client()
{
	char prefix[16] = {0}, dhcpv4_path[256] = {0}, buf[256] = {0}, tmp[64] = {0};
	int count = 0, client_num = 0, i = 0;
#ifdef ASUSWRT
	int wan_num = 1;
	char *wan_proto = NULL;

#ifdef RTCONFIG_DUALWAN
	wan_num = nvram_contains_word("wans_dualwan", "none") ? 1 : 2;
#endif

#else 	/* DSL_ASUSWRT */
	char wan_proto[8] = {0};
	int wan_index = 0;
#endif

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	snprintf(buf, sizeof(buf), "%s.ClientNumberOfEntries", CLIENT_DHCP);
	count = atoi(__get_parameter_value(buf, tmp));

	/* reset dhcpv4 client */
	if (sw_mode != tr_mode) {
		sprintf(dhcpv4_path, "%s.Client", CLIENT_DHCP);
		if (delete_all_instance(dhcpv4_path))
			tr_log(LOG_ERROR, "delete_all_instance failed: %s", dhcpv4_path);

		/* set Device.DHCPv4.ClientNumberOfEntries as 0 */
		__set_parameter_value(buf, "0");
	}

	if (sw_mode == SW_MODE_ROUTER)
	{

#ifdef ASUSWRT
	for (i = 0; i < wan_num; i++) {
		snprintf(prefix, sizeof(prefix), "wan%d_", i);
		wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
		if (!strcmp(wan_proto, "dhcp")
#ifdef RTCONFIG_XDSL
			|| !strcmp(wan_proto, "mer")
#endif
			)
			client_num++;
		else if (!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp")
#ifdef RTCONFIG_XDSL
			|| !strcmp(wan_proto, "pppoa")
#endif			
			) {
			if (nvram_get_int(strcat_r(prefix, "dhcpenable_x", tmp)) == 1)
				client_num++;
		}
	}

#else 	/* DSL_ASUSWRT */

	if (pri_wan_inst) {
		if (dsl_pri) {
#ifdef TCSUPPORT_WAN_ATM
			wan_index = WAN_ATM_INDEX;
#endif
#ifdef TCSUPPORT_WAN_PTM
			wan_index = WAN_PTM_INDEX;
#endif
		}
#ifdef TCSUPPORT_WAN_ETHER			
		else if (wan_pri)
			wan_index = WAN_ETHER_INDEX;
#endif
#ifdef RTCONFIG_USB_MODEM
		else if (usb_pri)
			wan_index = WAN_USB_INDEX;
#endif
		snprintf(prefix, sizeof(prefix), "wan%d_", wan_index);
		tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);

		if (!strcmp(wan_proto, "dhcp"))
			client_num++;
	}

	if (sec_wan_inst) {
		if (dsl_sec) {
#ifdef TCSUPPORT_WAN_ATM
			wan_index = WAN_ATM_INDEX;
#endif
#ifdef TCSUPPORT_WAN_PTM
			wan_index = WAN_PTM_INDEX;
#endif
		}
#ifdef TCSUPPORT_WAN_ETHER			
		else if (wan_sec)
			wan_index = WAN_ETHER_INDEX;
#endif
#ifdef RTCONFIG_USB_MODEM
		else if (usb_sec)
			wan_index = WAN_USB_INDEX;
#endif
		snprintf(prefix, sizeof(prefix), "wan%d_", wan_index);
		tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);

		if (!strcmp(wan_proto, "dhcp"))
			client_num++;
	}
#endif

	if (count == 0 || client_num != count) {
		/*if (client_num == 1)
			__set_parameter_value(buf, "1");
		else
			__set_parameter_value(buf, "2");
		*/
		snprintf(tmp, sizeof(tmp), "%d", client_num);
		__set_parameter_value(buf, tmp);

		/* delete all instance of Device.DHCPv4.Client */
		sprintf(dhcpv4_path, "%s.Client", CLIENT_DHCP);
		if (delete_all_instance(dhcpv4_path)) {
			tr_log(LOG_ERROR, "delete_all_instance failed: %s", dhcpv4_path);
			return -1;
		}

		for (i = 0; i < client_num; i++) {
			sprintf(buf, "%s.Client.", CLIENT_DHCP);
			if (add_object(buf, strlen(buf)) >= 9000) {
				tr_log(LOG_ERROR, "add_object failed: %s", buf);
				return -1;				
			}
		}
	}

	}	/* if (sw_mode == SW_MODE_ROUTER) */
	else if (sw_mode == SW_MODE_AP)
	{

		client_num = 1;
		if (count == 0 || client_num != count) {
			snprintf(tmp, sizeof(tmp), "%d", client_num);
			__set_parameter_value(buf, tmp);

			/* delete all instance of Device.DHCPv4.Client */
			sprintf(dhcpv4_path, "%s.Client", CLIENT_DHCP);
			if (delete_all_instance(dhcpv4_path))
				tr_log(LOG_ERROR, "delete_all_instance failed: %s", dhcpv4_path);

			/* create an instance of Device.DHCPv4.Client for LAN */
			sprintf(buf, "%s.Client.", CLIENT_DHCP);
			if (add_object(buf, strlen(buf)) >= 9000)
				tr_log(LOG_ERROR, "add_object failed: %s", buf);
		}

	}

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

int update_processor_info()
{
	FILE *fp;
	char *str = NULL;
	char buf[128] = {0}, tmp[5] = {0}, cpumode[10] = {0};
	int processornum = 0, count = 0, i = 0;

	fp = fopen("/proc/cpuinfo", "r");
	if (fp == NULL)
		return -1;

	memset(cpumode, 0, sizeof(cpumode));

	while ((str = fgets(buf, sizeof(buf), fp)) != NULL) {
		if (strncmp(str, "Processor", strlen("Processor")) == 0) {
			if (strstr(str, "ARM") != NULL)
				sprintf(cpumode, "arm");
		}

		if(strncmp(str, "processor", strlen("processor")) == 0) {
			processornum++;
			continue;
		}
		if(!strncmp(str, "cpu model", strlen("cpu model"))) {
			if (strstr(str, "MIPS") != NULL) {
				/* check cpu mode is big-endian or little-endian */
				int num = 0x04030201;
				char c = *(char *)(&num);
			
				if (c == 0x04 || c ==0x01) {
					if (c == 0x04)
						sprintf(cpumode, "mipseb");
					else
						sprintf(cpumode, "mipsel");
				}
				else {
					tr_log(LOG_DEBUG, "%s - get cpu mode failed", __FUNCTION__);
					return -1;
				}
			}
			else
				sprintf(cpumode, "arm");
		}
	}

	snprintf(buf, sizeof(buf), "%s", PROCESSOR_NUM);
	count = atoi(__get_parameter_value(buf, tmp));

	/* Device.DeviceInfo.ProcessorNumberOfEntries */
	if (count == 0 || count != processornum) {
		sprintf(tmp, "%d", processornum);
		__set_parameter_value(buf, tmp);

		/* delete all instance of Device.DeviceInfo.Processor */
		if (delete_all_instance(PROCESSOR)) {
			tr_log(LOG_ERROR, "delete_all_instance failed: %s", PROCESSOR);
			return -1;
		}

		for (i = 0; i < processornum; i++) {
			sprintf(buf, "%s.", PROCESSOR);
			if (add_object(buf, strlen(buf)) < 9000) {
				snprintf(buf, sizeof(buf), "%s.%d.Architecture", PROCESSOR, i + 1);
				__set_parameter_value(buf, cpumode);	
			}
			else {
				tr_log(LOG_ERROR, "add_object failed: %s", buf);
				return -1;
			}
		}
	}

	return 0;
}

char *wifinum_prefix_by_path(char *path, char *prefix)
{
	int unit = -1, subunit = -1, index = -1;

	if (strncmp(path, RADIO_2G, strlen(RADIO_2G)) == 0)	// 2.4G
		unit = 0;
	else if (strncmp(path, RADIO_5G, strlen(RADIO_5G)) == 0)// 5G
		unit = 1;
	else if (strncmp(path, RADIO ".SSID.", strlen(RADIO ".SSID.")) == 0)
		index = atoi(path + strlen(RADIO ".SSID."));
	else if (strncmp(path, RADIO ".AccessPoint.", strlen(RADIO ".AccessPoint.")) == 0)
		index = atoi(path + strlen(RADIO ".AccessPoint."));

	if (unit < 0) {
		if (index == START_INDEX_2G)		/* for 2.4G */
			unit = 0;
		else if (index == START_INDEX_5G)	/* for 5G */
			unit = 1;
		else if (index >= (START_INDEX_2G + 1) && index <= (START_INDEX_2G + NUM_MMSSID)) {			/* 2,3,4 for 2.4G */
			subunit = index - START_INDEX_2G;
			unit = 0;
		} else if (index >= (START_INDEX_5G + 1) && index <= (START_INDEX_5G + NUM_MMSSID)) {		/* 6,7,8 for 5G */
			subunit = index - START_INDEX_5G;
			unit = 1;
		} else 	/* TODO: should be an error */
			unit = 0;
	}

	if (subunit < 0)
		sprintf(prefix, "wl%d_", unit);
	else
		sprintf(prefix, "wl%d.%d_", unit, subunit);

	return prefix;
}

char *dhcpv4_client_by_path(char *path, char *prefix)
{
#ifdef ASUSWRT
	if (sw_mode == SW_MODE_ROUTER) {
		if (strncmp(path, CLIENT_DHCP_1, strlen(CLIENT_DHCP_1)) == 0)	//wan0_
			sprintf(prefix, "wan0_");
		else
			sprintf(prefix, "wan1_");
	}
	else if (sw_mode == SW_MODE_AP)
		sprintf(prefix, "lan_");

#else 	/* DSL_ASUSWRT */
	int wan_index = 0;

	if (strncmp(path, CLIENT_DHCP_1, strlen(CLIENT_DHCP_1)) == 0) {
		if (dsl_pri) {
#ifdef TCSUPPORT_WAN_ATM
			wan_index = WAN_ATM_INDEX;
#endif
#ifdef TCSUPPORT_WAN_PTM
			wan_index = WAN_PTM_INDEX;
#endif
		}
#ifdef TCSUPPORT_WAN_ETHER			
		else if (wan_pri)
			wan_index = WAN_ETHER_INDEX;
#endif
#ifdef RTCONFIG_USB_MODEM
		else if (usb_pri)
			wan_index = WAN_USB_INDEX;
#endif	
	}
	else
	{
		if (dsl_sec) {
#ifdef TCSUPPORT_WAN_ATM
			wan_index = WAN_ATM_INDEX;
#endif
#ifdef TCSUPPORT_WAN_PTM
			wan_index = WAN_PTM_INDEX;
#endif
		}
#ifdef TCSUPPORT_WAN_ETHER			
		else if (wan_sec)
			wan_index = WAN_ETHER_INDEX;
#endif
#ifdef RTCONFIG_USB_MODEM
		else if (usb_sec)
			wan_index = WAN_USB_INDEX;
#endif	
	}

	if (wan_index)
		sprintf(prefix, "wan%d_", wan_index);
#endif

	return prefix;
}

char *ethernet_prefix_by_path(char *path, char *prefix)
{
	node_t node;
	char buf[128] = {0};
	unsigned int eth_if_inst = 0;

	if (strstr(path, ETH_IF) == 0
#ifdef RTCONFIG_USB_MODEM
		&& strstr(path, USB_IF) == 0
#endif
#if defined(RTCONFIG_DSL) || defined(TCSUPPORT_WAN_ATM)
		&& strstr(path, ATM_LINK) == 0
#endif
#if defined(RTCONFIG_VDSL) || defined(TCSUPPORT_WAN_PTM)
		&& strstr(path, PTM_LINK) == 0
#endif
		) {
		if (lib_resolve_node(path, &node) == 0) {
			char lowerlayer[64] = {0};

			memset(lowerlayer, 0, sizeof(lowerlayer));

			if (strstr(path, PPP_IF))
				snprintf(buf, sizeof(buf), "%s.%d.LowerLayers", PPP_IF, getInstNum(path, "Interface"));
			else if (strstr(path, IP_IF))
				snprintf(buf, sizeof(buf), "%s.%d.LowerLayers", IP_IF, getInstNum(path, "Interface"));
			else
				return prefix; 

			__get_parameter_value(buf, lowerlayer);
			if (strlen(lowerlayer) == 0)
				return prefix; 

#ifdef ASUSWRT
			if (strstr(lowerlayer, DEVICE_BRIDGING_BRIDGE))
				sprintf(prefix, "lan_");
			else if (strstr(lowerlayer, ETH_IF)) {
				eth_if_inst = getInstNum(lowerlayer, "Interface");

				if (pri_wan_inst == eth_if_inst)
					sprintf(prefix, "wan0_");
#ifdef RTCONFIG_DUALWAN
				else if (sec_wan_inst == eth_if_inst)
					sprintf(prefix, "wan1_");
#endif
				else if (eth_if_inst == DEVICE_ETH_LAN_INST)
					sprintf(prefix, "lan_");
			}
#ifdef RTCONFIG_USB_MODEM
			else if (strstr(lowerlayer, USB_IF)) {
				if (pri_wan_inst == DEVICE_USB_INST)
					sprintf(prefix, "wan0_");
#ifdef RTCONFIG_DUALWAN
				else if (sec_wan_inst == DEVICE_USB_INST)
					sprintf(prefix, "wan1_");
#endif
			}
#endif	//RTCONFIG_USB_MODEM
#ifdef RTCONFIG_DSL
			else if (strstr(lowerlayer, ATM_LINK)) {
				//sprintf(prefix, "dsl%d", ADSL_INTERNET_INDEX);
				if (nvram_match("dslx_transmode", ADSL_TRANS_MODE)) {
					if (pri_wan_inst == DEVICE_DSL_INST)
						sprintf(prefix, "wan0_");
					else if (sec_wan_inst == DEVICE_DSL_INST)
						sprintf(prefix, "wan1_");
				}
			}
#endif
#ifdef RTCONFIG_VDSL
			else if (strstr(lowerlayer, PTM_LINK)) {
				//sprintf(prefix, "dsl%d", VDSL_INTERNET_INDEX);
				if (nvram_match("dslx_transmode", VDSL_TRANS_MODE)) {
					if (pri_wan_inst == DEVICE_DSL_INST)
						sprintf(prefix, "wan0_");
					else if (sec_wan_inst == DEVICE_DSL_INST)
						sprintf(prefix, "wan1_");				
				}
			}
#endif

#else 	/* DSL_ASUSWRT */
			if (strstr(lowerlayer, DEVICE_BRIDGING_BRIDGE))
				sprintf(prefix, "lan_");
			else if (strstr(lowerlayer, ETH_IF)) {
				eth_if_inst = getInstNum(lowerlayer, "Interface");

#ifdef TCSUPPORT_WAN_ETHER
				if (eth_if_inst == DEVICE_ETH_WAN_INST)
					sprintf(prefix, "wan%d_", WAN_ETHER_INDEX);
#endif				
				if (eth_if_inst == DEVICE_ETH_LAN_INST)
					sprintf(prefix, "lan_");
			}
#ifdef RTCONFIG_USB_MODEM
			else if (strstr(lowerlayer, USB_IF)) {
				sprintf(prefix, "wan%d_", WAN_USB_INDEX);
			}
#endif	/* RTCONFIG_USB_MODEM */
#ifdef TCSUPPORT_WAN_ATM
			else if (strstr(lowerlayer, ATM_LINK)) {
				sprintf(prefix, "wan%d_", WAN_ATM_INDEX);
			}
#endif
#ifdef TCSUPPORT_WAN_PTM
			else if (strstr(lowerlayer, PTM_LINK)) {
				sprintf(prefix, "wan%d_", WAN_PTM_INDEX);
			}
#endif
#endif
			else
				ethernet_prefix_by_path(lowerlayer, prefix);
		}
	}
	else
	{
#ifdef ASUSWRT
		if (strstr(path, ETH_IF)) {
			eth_if_inst = getInstNum(path, "Interface");

			if (pri_wan_inst == eth_if_inst)
				sprintf(prefix, "wan0_");
#ifdef RTCONFIG_DUALWAN
			else if (sec_wan_inst == eth_if_inst) 
				sprintf(prefix, "wan1_");
#endif
			else if (eth_if_inst == DEVICE_ETH_LAN_INST)
				sprintf(prefix, "lan_");
		}
#ifdef RTCONFIG_USB_MODEM
		else if (strstr(path, USB_IF)) {
			if (pri_wan_inst == DEVICE_USB_INST)
				sprintf(prefix, "wan0_");
#ifdef RTCONFIG_DUALWAN
			else if (sec_wan_inst == DEVICE_USB_INST)
				sprintf(prefix, "wan1_");
#endif
		}
#endif	//RTCONFIG_USB_MODEM
#ifdef RTCONFIG_DSL
		else if (strstr(path, ATM_LINK)) {
			//sprintf(prefix, "dsl%d", ADSL_INTERNET_INDEX);
			if (nvram_match("dslx_transmode", ADSL_TRANS_MODE)) {
				if (pri_wan_inst == DEVICE_DSL_INST)
					sprintf(prefix, "wan0_");
				else if (sec_wan_inst == DEVICE_DSL_INST)
					sprintf(prefix, "wan1_");
			}
		}
#endif
#ifdef RTCONFIG_VDSL
		else if (strstr(path, PTM_LINK)) {
			//sprintf(prefix, "dsl%d", VDSL_INTERNET_INDEX);
			if (nvram_match("dslx_transmode", VDSL_TRANS_MODE)) {
				if (pri_wan_inst == DEVICE_DSL_INST)
					sprintf(prefix, "wan0_");
				else if (sec_wan_inst == DEVICE_DSL_INST)
					sprintf(prefix, "wan1_");
			}
		}
#endif

#else 	/* DSL_ASUSWRT */
		if (strstr(path, ETH_IF)) {
			eth_if_inst = getInstNum(path, "Interface");

#ifdef TCSUPPORT_WAN_ETHER
			if (eth_if_inst == DEVICE_ETH_WAN_INST)
				sprintf(prefix, "wan%d_", WAN_ETHER_INDEX);
#endif				
			if (eth_if_inst == DEVICE_ETH_LAN_INST)
				sprintf(prefix, "lan_");
		}
#ifdef RTCONFIG_USB_MODEM
		else if (strstr(path, USB_IF)) {
			sprintf(prefix, "wan%d_", WAN_USB_INDEX);
		}
#endif	/* RTCONFIG_USB_MODEM */
#ifdef TCSUPPORT_WAN_ATM
		else if (strstr(path, ATM_LINK)) {
			sprintf(prefix, "wan%d_", WAN_ATM_INDEX);
		}
#endif
#ifdef TCSUPPORT_WAN_PTM
		else if (strstr(path, PTM_LINK)) {
			sprintf(prefix, "wan%d_", WAN_PTM_INDEX);
		}
#endif	
#endif
	}

	return prefix;
}

int ethernet_unit_by_path(char *path)
{
	char prefix[16] = {0}, tmp[32] = {0};
	int unit = -1;

	sprintf(prefix, "%s", ethernet_prefix_by_path(path, tmp));

	if(strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	if (!strncmp(prefix, "wan0", 4))
		unit = 0;
#ifdef RTCONFIG_DUALWAN
	else if (!strncmp(prefix, "wan1", 4))
		unit = 1;
#endif

#else 	/* DSL_ASUSWRT */
	char buf[16] = {0};
#ifdef TCSUPPORT_WAN_ATM
	snprintf(buf, sizeof(buf), "wan%d_", WAN_ATM_INDEX);
	if (!strcmp(prefix, buf)) unit = WAN_ATM_INDEX;
#endif

#ifdef TCSUPPORT_WAN_PTM
	snprintf(buf, sizeof(buf), "wan%d_", WAN_PTM_INDEX);
	if (!strcmp(prefix, buf)) unit = WAN_PTM_INDEX;
#endif

#ifdef TCSUPPORT_WAN_ETHER
	snprintf(buf, sizeof(buf), "wan%d_", WAN_ETHER_INDEX);
	if (!strcmp(prefix, buf)) unit = WAN_ETHER_INDEX;
#endif

#ifdef RTCONFIG_USB_MODEM
	snprintf(buf, sizeof(buf), "wan%d_", WAN_USB_INDEX);
	if (!strcmp(prefix, buf)) unit = WAN_USB_INDEX;
#endif
#endif
	else if (!strncmp(prefix, "lan", 3))
		unit = 2;

	return unit;
}

int search_ip_ppp_lowerlayer(char *path, int flag)
{
	node_t node;
	int count = 0;
	node_t *children = NULL;
	int found = 0;
	char buf[64] = {0};

	if (flag == IP_FLAG)
		snprintf(buf, sizeof(buf), "%s", IP_IF);
	else if (flag == PPP_FLAG)
		snprintf(buf, sizeof(buf), "%s", PPP_IF);
	else
		return -1;

	if (lib_resolve_node(buf, &node) == 0) {
		count = lib_get_children(node, &children);
		
		while (count > 0) {
			char name[16] = {0};
			count--;

			if (lib_get_property(children[count], "name", name) == 0) {
				char lowerlayers[128] = {0};

				memset(buf, 0, sizeof(buf));
				memset(lowerlayers, 0, sizeof(lowerlayers));
				
				if (flag == IP_FLAG)
					snprintf(buf, sizeof(buf), "%s.%s.LowerLayers", IP_IF, name);
				else if (flag == PPP_FLAG)
					snprintf(buf, sizeof(buf), "%s.%s.LowerLayers", PPP_IF, name);
				else
					break;

				__get_parameter_value(buf, lowerlayers);

				if (strlen(lowerlayers) && !strcmp(path, lowerlayers)) {
					found = atoi(name);
					break;
				}
			}
		}
				
		if (children) {
			lib_destroy_children(children);
			children = NULL;
		}
	}

	return found;
}

int search_bridging_lowerlayer(char *path)
{
	node_t node;
	int count = 0;
	node_t *children = NULL;
	int found = 0;
	char buf[64] = {0};

	snprintf(buf, sizeof(buf), "%s", IP_IF);

	if (lib_resolve_node(buf, &node) == 0) {
		count = lib_get_children(node, &children);
		
		while (count > 0) {
			char name[16] = {0};
			count--;

			if (lib_get_property(children[count], "name", name) == 0) {
				char lowerlayers[128] = {0};

				memset(buf, 0, sizeof(buf));
				memset(lowerlayers, 0, sizeof(lowerlayers));
				
				snprintf(buf, sizeof(buf), "%s.%s.LowerLayers", IP_IF, name);
				
				__get_parameter_value(buf, lowerlayers);

				if (strlen(lowerlayers) && !strcmp(path, lowerlayers)) {
					found = atoi(name);
					break;
				}
			}
		}
				
		if (children) {
			lib_destroy_children(children);
			children = NULL;
		}
	}
 
	return found;
}

void update_if_num(char *path, char *field_name)
{
	node_t node;
	int count = 0;
	node_t *children = NULL;
	char buf[64] = {0}, count_str[4] = {0};

	snprintf(buf, sizeof(buf), "%s.Interface", path);
	if (lib_resolve_node(buf, &node) == 0) {
		count = lib_get_children(node, &children);
				
		if (children) {
			lib_destroy_children(children);
			children = NULL;
		}

		snprintf(buf, sizeof(buf), "%s.%s", path, field_name);
		snprintf(count_str, sizeof(count_str), "%d", count);
		__set_parameter_value(buf, count_str);
	}
}

int add_ip_ppp_if_lowerlayer(int flag, char *lowerlayers)
{
	char buf[128] = {0};
	int res = -1;
	
	if (flag == IP_FLAG)
		snprintf(buf, sizeof(buf), "%s.", IP_IF);
	else if (flag == PPP_FLAG)
		snprintf(buf, sizeof(buf), "%s.", PPP_IF);
	else 
		return -1;

	if ((res = add_object(buf, strlen(buf))) >= 9000) {
		tr_log(LOG_ERROR, "add_object failed: %s", buf);
		return -1;
	}
	else 	/* set layerlayers of PPP/IP.Interface.X */
	{
		if (flag == IP_FLAG) {
			snprintf(buf, sizeof(buf), "%s.%d.LowerLayers", IP_IF, res);
			update_if_num(DEVICE_IP, "InterfaceNumberOfEntries");
		}
		else if (flag == PPP_FLAG) {
			snprintf(buf, sizeof(buf), "%s.%d.LowerLayers", PPP_IF, res);
			update_if_num(DEVICE_PPP, "InterfaceNumberOfEntries");
		}
		__set_parameter_value(buf, lowerlayers);
	}

	return res;
}

int check_ip_ppp_ethernet(char *prefix, int if_inst)
{
	char buf[256] = {0}, tmp[32] = {0};
#ifdef ASUSWRT
	char *wan_proto = NULL;

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
#else 	/* DSL_ASUSWRT */
	char wan_proto[8] = {0};

	tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);
#endif
	
	/* check IP for dhcp and static */
	if (!strcmp(wan_proto, "dhcp") || !strcmp(wan_proto, "static")
#ifdef ASUSWRT
	 	|| nvram_match(strcat_r(prefix, "vpndhcp", tmp), "1")
#endif
	 ) {
#ifdef RTCONFIG_USB_MODEM
		if (if_inst == DEVICE_USB_INST)
			snprintf(buf, sizeof(buf), "%s", USB_IF_WAN);
		else
#endif
		snprintf(buf, sizeof(buf), "%s.%d", ETH_IF, if_inst);
		if (search_ip_ppp_lowerlayer(buf, IP_FLAG) == 0)
			add_ip_ppp_if_lowerlayer(IP_FLAG, buf);
	}
	
	/* check IP and PPP for pppoe, pptp, l2tp */
	if (!strcmp(wan_proto, "pppoe")
#ifdef ASUSWRT
		 || !strcmp(wan_proto, "pptp") || !strcmp(wan_proto, "l2tp")
#endif
	) {
#ifdef RTCONFIG_USB_MODEM
		if (if_inst == DEVICE_USB_INST)
			snprintf(buf, sizeof(buf), "%s", USB_IF_WAN);
		else
#endif		
		snprintf(buf, sizeof(buf), "%s.%d", ETH_IF, if_inst);
		if (search_ip_ppp_lowerlayer(buf, PPP_FLAG) == 0) {
			int ppp_if_inst = 0;

			ppp_if_inst = add_ip_ppp_if_lowerlayer(PPP_FLAG, buf);
			snprintf(buf, sizeof(buf), "%s.%d", PPP_IF, ppp_if_inst);
			if (search_ip_ppp_lowerlayer(buf, IP_FLAG) == 0)
				add_ip_ppp_if_lowerlayer(IP_FLAG, buf);
		}
	}

	return 0;
}

int update_wan_if_upstream()
{
	node_t node;
	int count = 0;
	node_t *children = NULL;
	char buf[64] = {0};

	/* update for Ethernet if */
	if (lib_resolve_node(ETH_IF, &node) == 0) {
		count = lib_get_children(node, &children);
		
		while (count > 0) {
			char name[16] = {0};
			count--;

			if (lib_get_property(children[count], "name", name) == 0) {
				snprintf(buf, sizeof(buf), "%s.%s.Upstream", ETH_IF, name);
				if (pri_wan_inst == atoi(name) || sec_wan_inst == atoi(name))
					__set_parameter_value(buf, "true");
				else
					__set_parameter_value(buf, "false");
			}
		}
				
		if (children) {
			lib_destroy_children(children);
			children = NULL;
		}
	}

#ifdef RTCONFIG_USB_MODEM
	/* update for USB if */
	snprintf(buf, sizeof(buf), "%s.Upstream", USB_IF_WAN);
	if (pri_wan_inst == DEVICE_USB_INST || sec_wan_inst == DEVICE_USB_INST) {
		int is_dualwan = 0;
#ifdef ASUSWRT
		int wan_primary_unit = wan_primary_ifunit();
#else 	/* DSL_ASUSWRT */
		int wan_primary_unit = tcapi_get_int(WANDUCK_NODE, "wan_primary");
#endif

#ifdef RTCONFIG_DUALWAN
#ifdef ASUSWRT
		if (!strstr(nvram_get("wans_dualwan"), "none"))
			is_dualwan = 1;
#else 	/* DSL_ASUSWRT */
		char wans_dualwan[32] = {0};

		tcapi_get(DUALWAN_NODE, "wans_dualwan", wans_dualwan);
		if(!strstr(wans_dualwan, "none"))
			is_dualwan = 1;	
#endif
#endif

#ifdef ASUSWRT
		if ((is_dualwan && nvram_match("wans_mode", "lb")) || 
			(pri_wan_inst == DEVICE_USB_INST && wan_primary_unit == WAN_UNIT_FIRST) ||
			(sec_wan_inst == DEVICE_USB_INST && wan_primary_unit == WAN_UNIT_SECOND))
#else 	/* DSL_ASUSWRT */
		char wans_mode[8] = {0};
		int wan_index = 0;

		if (dsl_pri) {
#ifdef TCSUPPORT_WAN_ATM
			wan_index = WAN_ATM_INDEX;
#endif
#ifdef TCSUPPORT_WAN_PTM
			wan_index = WAN_PTM_INDEX;
#endif
		}
#ifdef TCSUPPORT_WAN_ETHER			
		else if (wan_pri) {
			wan_index = WAN_ETHER_INDEX;
		}
#endif
#ifdef RTCONFIG_USB_MODEM
		else if (usb_pri) {
			wan_index = WAN_USB_INDEX;
		}
#endif

		tcapi_get(DUALWAN_NODE, "wans_dualwan", wans_dualwan);
		if(!strstr(wans_dualwan, "none"))
			is_dualwan = 1;		

		tcapi_get(DUALWAN_NODE, "wans_mode", wans_mode);
		if ((is_dualwan && !strcmp(wans_mode, "lb")) || 
			(pri_wan_inst == DEVICE_USB_INST && wan_primary_unit == wan_index) ||
			(sec_wan_inst == DEVICE_USB_INST && wan_primary_unit == wan_index))	
#endif
		{
			__set_parameter_value(buf, "true");
		}
		else
		{
			__set_parameter_value(buf, "false");
		}
	}
	else
		__set_parameter_value(buf, "false");
#endif

	return 0;	
}

#ifdef RTCONFIG_XDSL
void update_xtm_link_num(char *path, char *field_name)
{
	node_t node;
	int count = 0;
	node_t *children = NULL;
	char buf[64] = {0}, count_str[4] = {0};

	snprintf(buf, sizeof(buf), "%s.Link", path);
	if (lib_resolve_node(buf, &node) == 0) {
		count = lib_get_children(node, &children);
				
		if (children) {
			lib_destroy_children(children);
			children = NULL;
		}

		snprintf(buf, sizeof(buf), "%s.%s", path, field_name);
		snprintf(count_str, sizeof(count_str), "%d", count);
		__set_parameter_value(buf, count_str);
	}
}

static int add_atm_link(node_t node, char *arg, int nin)
{
	/* update InterfaceNumberOfEntries for Device.ATM */
	update_xtm_link_num(DEVICE_ATM, "LinkNumberOfEntries");
	return 0;
}

static int del_atm_link(node_t node, char *arg, int nin)
{
	char prefix[sizeof("dslXXXXXXXXXX")] = {0}, buf[128] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
#endif
	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';

	tr_log(LOG_DEBUG, "%s - path=%s, nin=%d", __FUNCTION__, path, nin);

	/* disable the corresponding dsl when wcd deleted */
	snprintf(buf, sizeof(buf), "%s.%d.X_ASUS_DSLIndex", path, nin);
	__get_parameter_value(buf, prefix);
	if (strlen(prefix))
#ifdef ASUSWRT
		nvram_set(strcat_r(prefix, "_enable", tmp), "0");
#else 	/* DSL_ASUSWRT */
		tcapi_set(prefix, "ACTIVE", "No");
#endif
	
	/* update InterfaceNumberOfEntries for Device.ATM */
	update_xtm_link_num(DEVICE_ATM, "LinkNumberOfEntries");
	return 0;
}

static int add_ptm_link(node_t node, char *arg, int nin)
{
	/* update InterfaceNumberOfEntries for Device.PTM */
	update_xtm_link_num(DEVICE_PTM, "LinkNumberOfEntries");
	return 0;
}

static int del_ptm_link(node_t node, char *arg, int nin)
{
	char prefix[sizeof("dslXXXXXXXXXX")] = {0}, buf[128] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
#endif
	char *path = lib_node2path(node);

	memset(prefix, 0, sizeof(prefix));
	path[strlen(path) - 1] = '\0';

	tr_log(LOG_DEBUG, "%s - path=%s, nin=%d", __FUNCTION__, path, nin);

	/* disable the corresponding dsl when wcd deleted */
	snprintf(buf, sizeof(buf), "%s.%d.X_ASUS_DSLIndex", path, nin);
	__get_parameter_value(buf, prefix);
	if (strlen(prefix))
#ifdef ASUSWRT
		nvram_set(strcat_r(prefix, "_enable", tmp), "0");
#else 	/* DSL_ASUSWRT */
		tcapi_set(prefix, "ACTIVE", "No");
#endif

	/* update InterfaceNumberOfEntries for Device.PTM */
	update_xtm_link_num(DEVICE_PTM, "LinkNumberOfEntries");
	return 0;
}

int add_xtm_link(int xdsl_id, char *prefix, int i)
{
	int inst_index = 0, result = 0, ppp_if_inst = 0;
	char buf[256] = {0};
#ifdef ASUSWRT
	char tmp[32] = {0};
	char *dsl_proto = NULL;
#else 	/* DSL_ASUSWRT */
	char dsl_proto[9] = {0};
#endif

#if defined(RTCONFIG_VDSL) || defined(TCSUPPORT_WAN_PTM)
	if (xdsl_id == VDSL_ID)
		snprintf(buf, sizeof(buf), "%s.", PTM_LINK);
	else
#endif
	snprintf(buf, sizeof(buf), "%s.", ATM_LINK);
	if ((inst_index = add_object(buf, strlen(buf))) < 9000) {
#if defined(RTCONFIG_VDSL) || defined(TCSUPPORT_WAN_PTM)
		if (xdsl_id == VDSL_ID)
			snprintf(buf, sizeof(buf), "%s.%d.X_ASUS_DSLIndex", PTM_LINK, inst_index);
		else
#endif		
		snprintf(buf, sizeof(buf), "%s.%d.X_ASUS_DSLIndex", ATM_LINK, inst_index);
		__set_parameter_value(buf, prefix);
		
		if (i == 0) {	/* the link of ATM/PTM for internet */
#ifdef ASUSWRT
			dsl_proto = nvram_safe_get(strcat_r(prefix, "_proto", tmp));

			/* create IP if for ipoa, mer */
			if (!strcmp(dsl_proto, "dhcp") || !strcmp(dsl_proto, "static")
#ifdef RTCONFIG_VDSL				
				|| !strcmp(dsl_proto, "ipoa") || !strcmp(dsl_proto, "mer")
#endif
				)

#else 	/* DSL_ASUSWRT */
			tcapi_get(prefix, "ISP", dsl_proto);
			if (!strcmp(dsl_proto, "0") || !strcmp(dsl_proto, "1"))
#endif
			{
#if defined(RTCONFIG_VDSL) || defined(TCSUPPORT_WAN_PTM)
				if (xdsl_id == VDSL_ID)
					snprintf(buf, sizeof(buf), "%s.%d", PTM_LINK, inst_index);
				else
#endif
				snprintf(buf, sizeof(buf), "%s.%d", ATM_LINK, inst_index);
				if (search_ip_ppp_lowerlayer(buf, IP_FLAG) == 0)
					add_ip_ppp_if_lowerlayer(IP_FLAG, buf);
			}

			/* create PPP for pppoe, pppoa */
#ifdef ASUSWRT
			if (!strcmp(dsl_proto, "pppoe")	|| !strcmp(dsl_proto, "pppoa")) 
#else 	/* DSL_ASUSWRT */
			if (!strcmp(dsl_proto, "2"))
#endif
			{
#if defined(RTCONFIG_VDSL) || defined(TCSUPPORT_WAN_PTM)
				if (xdsl_id == VDSL_ID)
					snprintf(buf, sizeof(buf), "%s.%d", PTM_LINK, inst_index);
				else
#endif
				snprintf(buf, sizeof(buf), "%s.%d", ATM_LINK, inst_index);
				if (search_ip_ppp_lowerlayer(buf, PPP_FLAG) == 0) {
					ppp_if_inst = add_ip_ppp_if_lowerlayer(PPP_FLAG, buf);
					snprintf(buf, sizeof(buf), "%s.%d", PPP_IF, ppp_if_inst);
					if (search_ip_ppp_lowerlayer(buf, IP_FLAG) == 0)
						add_ip_ppp_if_lowerlayer(IP_FLAG, buf);
				}
			}
		}

		result = 1;
	}
	else
		tr_log(LOG_DEBUG, "%s - fail to add object (%s)", __FUNCTION__, buf);

	return result;
}

int find_xtm_link(int xdsl_id, char *index_name)
{
	char buf[256] = {0};
	node_t *children = NULL;
	int count = 0, match_link_inst = 0;
	node_t node;

#if defined(RTCONFIG_VDSL) || defined(TCSUPPORT_WAN_PTM)
	if (xdsl_id == VDSL_ID)
		snprintf(buf, sizeof(buf), "%s", PTM_LINK);
	else
#endif
	snprintf(buf, sizeof(buf), "%s", ATM_LINK);

	if (lib_resolve_node(buf, &node) == 0) {
		count = lib_get_children(node, &children);
		
		while (count > 0) {
			char name[8] = {0}, dsl_index[8] = {0};

			count--;
			if (lib_get_property(children[count], "name", name) == 0) {
				if (string_is_digits(name)) {
#if defined(RTCONFIG_VDSL) || defined(TCSUPPORT_WAN_PTM)
					if (xdsl_id == VDSL_ID)
						snprintf(buf, sizeof(buf), "%s.%s.X_ASUS_DSLIndex", PTM_LINK, name);
					else
#endif					
					snprintf(buf, sizeof(buf), "%s.%s.X_ASUS_DSLIndex", ATM_LINK, name);
					__get_parameter_value(buf, dsl_index);

					if (!strcmp(index_name, dsl_index))	{ /* find it */
						match_link_inst = atoi(name);
						break;
					}
				}
			}
		}
				
		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}
	}

	return match_link_inst;
}

int update_xtm_link(int xdsl_id)
{
	char buf[256] = {0}, prefix[sizeof("dslXXXXXXXXXX_")] = {0}, count_str[4] = {0};
	node_t *children = NULL;
	node_t node;
#ifdef ASUSWRT
	char tmp[32] = {0};
	char *dsl_proto = NULL;
#else 	/* DSL_ASUSWRT */
	char dsl_proto[8] = {0};
#endif
	int link_count = 0, inst_index = 0, i = 0, ip_if_inst = 0, ppp_if_inst = 0;
		
	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	/* count the number of Device.ATM.LinkNumberOfEntries */
#if defined(RTCONFIG_VDSL) || defined(TCSUPPORT_WAN_PTM)
	if (xdsl_id == VDSL_ID)
		snprintf(buf, sizeof(buf), "%s", PTM_LINK);
	else
#endif
	snprintf(buf, sizeof(buf), "%s", ATM_LINK);
	if (lib_resolve_node(buf, &node) == 0) {
		link_count = lib_get_children(node, &children);
		if (children) {
			lib_destroy_children(children);
			children = NULL;
		}
	}
	else
	{
		tr_log(LOG_DEBUG, "%s - can't count the number of xTM", __FUNCTION__);
		return -1;
	}

	if (link_count == 0) {
		/* create link for ADSL (ATM) or VDSL (PTM) */
		for (i = 0; i < DSL_NUM; i++) {
#ifdef ASUSWRT
#ifdef RTCONFIG_VDSL
			if (xdsl_id == VDSL_ID) {
				if (i == 0)
					snprintf(prefix, sizeof(prefix), "dsl8");
				else
					snprintf(prefix, sizeof(prefix), "dsl8.%d", i);
			}
			else
#endif
			snprintf(prefix, sizeof(prefix), "dsl%d", i);
			if (nvram_match(strcat_r(prefix, "_enable", tmp), "1"))

#else 	/* DSL_ASUSWRT */
#ifdef TCSUPPORT_WAN_PTM
			if (xdsl_id == VDSL_ID) {
				if (i == 0)
					snprintf(prefix, sizeof(prefix), "%s8", WAN_XTM_PREFIX);
				else	
					snprintf(prefix, sizeof(prefix), "%s%d", WAN_PTM_EXT_PREFIX, i - 1);
			}
			else
#endif
			snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, i);
			if (tcapi_match(prefix, "Active", "Yes"))
#endif
			{
				if (add_xtm_link(xdsl_id, prefix, i))
					link_count++;
			}
		}
	}
	else
	{
		/* check whether add or delete IP&PPP for ADSL (ATM)/VDSL (PTM) */
		for (i = 0; i < DSL_NUM; i++) {
#ifdef ASUSWRT
#ifdef RTCONFIG_VDSL
			if (xdsl_id == VDSL_ID) { 
				if (i == 0)
					snprintf(prefix, sizeof(prefix), "dsl8");
				else
					snprintf(prefix, sizeof(prefix), "dsl8.%d", i);
			}
			else
#endif			
			snprintf(prefix, sizeof(prefix), "dsl%d", i);

			if (nvram_match(strcat_r(prefix, "_enable", tmp), "1"))

#else 	/* DSL_ASUSWRT */
#ifdef TCSUPPORT_WAN_PTM
			if (xdsl_id == VDSL_ID) {
				if (i == 0)
					snprintf(prefix, sizeof(prefix), "%s8", WAN_XTM_PREFIX);
				else	
					snprintf(prefix, sizeof(prefix), "%s%d", WAN_PTM_EXT_PREFIX, i - 1);
			}
			else
#endif
			snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, i);
			if (tcapi_match(prefix, "Active", "Yes"))
#endif
			{
				if (!find_xtm_link(xdsl_id, prefix)) {
					if (add_xtm_link(xdsl_id, prefix, i))
						link_count++;
				}
			}			
		}
	}

	/* set LinkNumberOfEntries of ATM/PTM */
#if defined(RTCONFIG_VDSL) || defined(TCSUPPORT_WAN_PTM)
	if (xdsl_id == VDSL_ID)	
		snprintf(buf, sizeof(buf), "%s", PTM_LINK_NUM);
#endif
	snprintf(buf, sizeof(buf), "%s", ATM_LINK_NUM);
	snprintf(count_str, sizeof(count_str), "%d", link_count);
	__set_parameter_value(buf, count_str);

	/* set static inform for ExternalIPAddress of WANIPConnection/WANPPPConnection */
#if defined(RTCONFIG_VDSL) || defined(TCSUPPORT_WAN_PTM)
	if (xdsl_id == VDSL_ID) {
#ifdef ASUSWRT
		if (nvram_match("dslx_transmode", VDSL_TRANS_MODE)) {
			snprintf(prefix, sizeof(prefix), "dsl%d", VDSL_INTERNET_INDEX);
#else 	/* DSL_ASUSWRT */
		if (tcapi_match(WAN_COMMON_NODE, "DSLMode", VDSL_TRANS_MODE)) {
			snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, VDSL_INTERNET_INDEX);
#endif

			if ((inst_index = find_xtm_link(xdsl_id, prefix))) {
				snprintf(buf, sizeof(buf), "%s.%d", PTM_LINK, inst_index);	
#ifdef ASUSWRT	
				dsl_proto = nvram_safe_get(strcat_r(prefix, "_proto", tmp));
				if (!strcmp(dsl_proto, "dhcp") || !strcmp(dsl_proto, "static"))
#else 	/* DSL_ASUSWRT */
				tcapi_get(prefix, "ISP", dsl_proto);
				if (!strcmp(dsl_proto, "0") || !strcmp(dsl_proto, "1"))	/* dhcp/static */
#endif
				{
					if ((ip_if_inst = search_ip_ppp_lowerlayer(buf, IP_FLAG))) {
						snprintf(buf, sizeof(buf), "%s.%d.IPv4Address.1.IPAddress", IP_IF, ip_if_inst);
						add_inform_parameter(buf, 0);
					}
				}
#ifdef ASUSWRT	
				else if (!strcmp(dsl_proto, "pppoe"))
#else 	/* DSL_ASUSWRT */
				else if (!strcmp(dsl_proto, "2")
#endif
				{
					if ((ppp_if_inst = search_ip_ppp_lowerlayer(buf, PPP_FLAG))) {
						snprintf(buf, sizeof(buf), "%s.%d", PPP_IF, ppp_if_inst);
						if ((ip_if_inst = search_ip_ppp_lowerlayer(buf, IP_FLAG))) {
							snprintf(buf, sizeof(buf), "%s.%d.IPv4Address.1.IPAddress", IP_IF, ip_if_inst);
							add_inform_parameter(buf, 0);
						}
					}
				}
			}
		}
	}
	else
#endif
	{
#ifdef ASUSWRT
		if (nvram_match("dslx_transmode", ADSL_TRANS_MODE)) {
			snprintf(prefix, sizeof(prefix), "dsl%d", ADSL_INTERNET_INDEX);

#else 	/* DSL_ASUSWRT */
		if (tcapi_match(WAN_COMMON_NODE, "DSLMode", ADSL_TRANS_MODE)) {
			snprintf(prefix, sizeof(prefix), "%s%d", WAN_XTM_PREFIX, ADSL_INTERNET_INDEX);
#endif

			if ((inst_index = find_xtm_link(xdsl_id, prefix))) {
				snprintf(buf, sizeof(buf), "%s.%d", ATM_LINK, inst_index);
#ifdef ASUSWRT
				dsl_proto = nvram_safe_get(strcat_r(prefix, "_proto", tmp));
				if (!strcmp(dsl_proto, "ipoa") || !strcmp(dsl_proto, "mer")) 
#else 	/* DSL_ASUSWRT */
				tcapi_get(prefix, "ISP", dsl_proto);
				if (!strcmp(dsl_proto, "0") || !strcmp(dsl_proto, "1"))	/* dhcp/static */
#endif
				{
					if ((ip_if_inst = search_ip_ppp_lowerlayer(buf, IP_FLAG))) {
						snprintf(buf, sizeof(buf), "%s.%d.IPv4Address.1.IPAddress", IP_IF, ip_if_inst);
						add_inform_parameter(buf, 0);
					}
				}
#ifdef ASUSWRT
				else if (!strcmp(dsl_proto, "pppoe") || !strcmp(dsl_proto, "pppoa"))
#else 	/* DSL_ASUSWRT */
				else if (!strcmp(dsl_proto, "2"))
#endif
				{
					if ((ppp_if_inst = search_ip_ppp_lowerlayer(buf, PPP_FLAG))) {
						snprintf(buf, sizeof(buf), "%s.%d", PPP_IF, ppp_if_inst);
						if ((ip_if_inst = search_ip_ppp_lowerlayer(buf, IP_FLAG))) {
							snprintf(buf, sizeof(buf), "%s.%d.IPv4Address.1.IPAddress", IP_IF, ip_if_inst);
							add_inform_parameter(buf, 0);
						}
					}
				}
			}
		}
	}

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

int update_xdsl_line(int wan_inst)
{
	int res = 0;

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	res = update_xtm_link(ADSL_ID);
#if defined(RTCONFIG_VDSL) || defined(TCSUPPORT_WAN_PTM)
	if (res != 0)
		return res;
	res = update_xtm_link(VDSL_ID);
#endif
	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return res;
}
#endif	/* end of RTCONFIG_XDSL */


void reset_interface()
{
	char buf[256] = {0};

	/* delete all instance of Device.IP.Interface */
	snprintf(buf, sizeof(buf), "%s.Interface", DEVICE_IP);
	if(delete_all_instance(buf))
		tr_log(LOG_ERROR, "delete_all_instance failed: %s", buf);
	
	/* reset the count of Device.IP.InterfaceNumberOfEntries as 0 */
	snprintf(buf, sizeof(buf), "%s.InterfaceNumberOfEntries", DEVICE_IP);
	__set_parameter_value(buf, "0");


	/* delete all instance of Device.PPP.Interface */
	snprintf(buf, sizeof(buf), "%s.Interface", DEVICE_PPP);
	if(delete_all_instance(buf))
		tr_log(LOG_ERROR, "delete_all_instance failed: %s", buf);

	/* reset the count of Device.PPP.InterfaceNumberOfEntries as 0 */
	snprintf(buf, sizeof(buf), "%s.InterfaceNumberOfEntries", DEVICE_PPP);
	__set_parameter_value(buf, "0");


	/* delete all instance of Device.Ethernet.Interface */
	snprintf(buf, sizeof(buf), "%s.Interface", DEVICE_ETHERNET);
	if(delete_all_instance(buf))
		tr_log(LOG_ERROR, "delete_all_instance failed: %s", buf);

	/* reset the count of Device.Ethernet.InterfaceNumberOfEntries as 0 */
	snprintf(buf, sizeof(buf), "%s.InterfaceNumberOfEntries", DEVICE_ETHERNET);
	__set_parameter_value(buf, "0");		
}

int update_wan_lan_if()
{
	char tmp[32] = {0}, buf[256] = {0}, prefix[sizeof("wanXXXXXXXXXX_")] = {0}, eth_if_num_str[4];
#ifdef ASUSWRT
	char *wan_proto = NULL;
	int wan_primary_unit = wan_primary_ifunit();
#else 	/* DSL_ASUSWRT */
	char wan_proto[8] = {0};
	int wan_primary_unit = tcapi_get_int(WANDUCK_NODE, "wan_primary");
#endif
	int ip_if_inst = 0, ppp_if_inst = 0, i = 0;
#ifndef RTCONFIG_XDSL
	int eth_if_num = 1;	/* ethernet if for wan as default */
#else
#ifdef ASUSWRT
	int eth_if_num = 0; /* no ethernet if for wan */
#else 	/* DSL_ASUSWRT */
	int eth_if_num = 1;	/* ethernet if for wan as default */
#endif 
#endif
	
	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

#ifdef RTCONFIG_DUALWAN
#ifdef ASUSWRT	
	eth_if_num += 2;	/* ethernet if for wanlan and lan */
#else 	/* DSL_ASUSWRT */
	eth_if_num++;		/* ethernet if for lan, no wanlan */
#endif
#else
	eth_if_num++;		/* ethernet if for lan */	
#endif	

	if (sw_mode == SW_MODE_ROUTER) {	/* router mode */

	if (sw_mode != tr_mode)	/* reset interface */
		reset_interface();

	/* check and create Ethernet.Interface.X */
	__get_parameter_value(ETH_IF_NUM, eth_if_num_str);
	if (atoi(eth_if_num_str) == 0) {
		snprintf(buf, sizeof(buf), "%s.", ETH_IF);
		for (i = 0; i < eth_if_num; i++) {
			if (add_object(buf, strlen(buf)) >= 9000) {
				tr_log(LOG_ERROR, "add_object failed: %s", buf);
				return -1;
			}
		}
	}

	/* update info first*/
	update_wan_if_upstream();
	update_if_num(DEVICE_IP, "InterfaceNumberOfEntries");
	update_if_num(DEVICE_PPP, "InterfaceNumberOfEntries");

	if (support_dualwan) {	/* support dual wan */
		int is_dualwan = 0;

#ifdef RTCONFIG_DUALWAN
#ifdef ASUSWRT
		if (!strstr(nvram_get("wans_dualwan"), "none"))
			is_dualwan = 1;

#else 	/* DSL_ASUSWRT */
		char wans_dualwan[32] = {0};

		tcapi_get(DUALWAN_NODE, "wans_dualwan", wans_dualwan);
		if(!strstr(wans_dualwan, "none"))
			is_dualwan = 1;
#endif
#endif

		if (pri_wan_inst) {
#ifdef ASUSWRT
#ifdef RTCONFIG_XDSL
			if (wan_pri) {	/* update for dsl wan */
				update_xdsl_line(pri_wan_inst);
			}
			else 	/* update for lan or usb wan */
#endif
			{
				snprintf(prefix, sizeof(prefix), "wan0_");

				/* check IP and PPP for primary wan */
				check_ip_ppp_ethernet(prefix, pri_wan_inst);

				if ((is_dualwan && nvram_match("wans_mode", "lb")) || wan_primary_unit == WAN_UNIT_FIRST) {
					/* set static inform for IPAddress of IP */
					wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

#else 	/* DSL_ASUSWRT */
#ifdef RTCONFIG_XDSL
			if (dsl_pri) {	/* update for dsl wan */
				update_xdsl_line(pri_wan_inst);
			}
			else 	/* update for lan or usb wan */
#endif	
			{
				char wans_mode[8] = {0};
				int wan_index = 0;

				if (dsl_pri) {
#ifdef TCSUPPORT_WAN_ATM
					wan_index = WAN_ATM_INDEX;
#endif
#ifdef TCSUPPORT_WAN_PTM
					wan_index = WAN_PTM_INDEX;
#endif
				}
#ifdef TCSUPPORT_WAN_ETHER			
				else if (wan_pri) {
					wan_index = WAN_ETHER_INDEX;
				}
#endif
#ifdef RTCONFIG_USB_MODEM
				else if (usb_pri) {
					wan_index = WAN_USB_INDEX;
				}
#endif

				tcapi_get(DUALWAN_NODE, "wans_mode", wans_mode);

				/* check IP and PPP for primary wan */
				snprintf(prefix, sizeof(prefix), "wan%d_", wan_index);
				check_ip_ppp_ethernet(prefix, pri_wan_inst);

				if ((is_dualwan && !strcmp(wans_mode, "lb")) || wan_primary_unit == wan_index) {	
					/* set static inform for ExternalIPAddress of WANIPConnection/WANPPPConnection */
					tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);	
#endif

					if (!strcmp(wan_proto, "dhcp") || !strcmp(wan_proto, "static")) {
						snprintf(buf, sizeof(buf), "%s.%d", ETH_IF, pri_wan_inst);
						if ((ip_if_inst = search_ip_ppp_lowerlayer(buf, IP_FLAG))) {
							snprintf(buf, sizeof(buf), "%s.%d.IPv4Address.1.IPAddress", IP_IF, ip_if_inst);
							add_inform_parameter(buf, 0);
						}
					}
					else
					{
						snprintf(buf, sizeof(buf), "%s.%d", ETH_IF, pri_wan_inst);
						if ((ppp_if_inst = search_ip_ppp_lowerlayer(buf, PPP_FLAG))) {
							snprintf(buf, sizeof(buf), "%s.%d", PPP_IF, ppp_if_inst);
							if ((ip_if_inst = search_ip_ppp_lowerlayer(buf, IP_FLAG))) {
								snprintf(buf, sizeof(buf), "%s.%d.IPv4Address.1.IPAddress", IP_IF, ip_if_inst);
								add_inform_parameter(buf, 0);
							}
						}
					}
				}
			}
		}

		if (sec_wan_inst) {
#ifdef ASUSWRT
#ifdef RTCONFIG_XDSL
			if (wan_sec) {	/* update for dsl wan */
				update_xdsl_line(sec_wan_inst);
			}
			else 	/* update for lan or usb wan */
#endif
			{			
				snprintf(prefix, sizeof(prefix), "wan1_");

				/* check IP and PPP for secondary wan */
				check_ip_ppp_ethernet(prefix, sec_wan_inst);

				if ((is_dualwan && nvram_match("wans_mode", "lb")) || wan_primary_unit == WAN_UNIT_SECOND) {
					/* set static inform for IPAddress of IP */
					wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

#else 	/* DSL_ASUSWRT */
#ifdef RTCONFIG_XDSL
			if (dsl_sec) {	/* update for dsl wan */
				update_xdsl_line(sec_wan_inst);
			}
			else 	/* update for lan or usb wan */
#endif
			{
				char wans_mode[8] = {0};
				int wan_index = 0;

				if (dsl_sec) {
#ifdef TCSUPPORT_WAN_ATM
					wan_index = WAN_ATM_INDEX;
#endif
#ifdef TCSUPPORT_WAN_PTM
					wan_index = WAN_PTM_INDEX;
#endif
				}
#ifdef TCSUPPORT_WAN_ETHER			
				else if (wan_sec)
					wan_index = WAN_ETHER_INDEX;
#endif
#ifdef RTCONFIG_USB_MODEM
				else if (usb_sec)
					wan_index = WAN_USB_INDEX;
#endif

				tcapi_get(DUALWAN_NODE, "wans_mode", wans_mode);

				/* check IP and PPP for secondary wan */
				snprintf(prefix, sizeof(prefix), "wan%d_", wan_index);
				check_ip_ppp_ethernet(prefix, sec_wan_inst);

				if ((is_dualwan && !strcmp(wans_mode, "lb")) || wan_primary_unit == wan_index) {
					/* set static inform for ExternalIPAddress of WANIPConnection/WANPPPConnection */
					tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);	
#endif
					if (!strcmp(wan_proto, "dhcp") || !strcmp(wan_proto, "static")) {
						snprintf(buf, sizeof(buf), "%s.%d", ETH_IF, sec_wan_inst);
						if ((ip_if_inst = search_ip_ppp_lowerlayer(buf, IP_FLAG))) {
							snprintf(buf, sizeof(buf), "%s.%d.IPv4Address.1.IPAddress", IP_IF, ip_if_inst);
							add_inform_parameter(buf, 0);
						}
					}
					else
					{
						snprintf(buf, sizeof(buf), "%s.%d", ETH_IF, sec_wan_inst);
						if ((ppp_if_inst = search_ip_ppp_lowerlayer(buf, PPP_FLAG))) {
							snprintf(buf, sizeof(buf), "%s.%d", PPP_IF, ppp_if_inst);
							if ((ip_if_inst = search_ip_ppp_lowerlayer(buf, IP_FLAG))) {
								snprintf(buf, sizeof(buf), "%s.%d.IPv4Address.1.IPAddress", IP_IF, ip_if_inst);
								add_inform_parameter(buf, 0);
							}
						}
					}
				}
			}
		}

		/* set Device.Ethernet.InterfaceNumberOfEntries as wan or dsl + wanlan + lan */
		snprintf(eth_if_num_str, sizeof(eth_if_num_str), "%d", eth_if_num);
		__set_parameter_value(ETH_IF_NUM, eth_if_num_str);
	}
	else /* doesn't support dual wan */
	{
#ifdef RTCONFIG_XDSL
		update_xdsl_line(pri_wan_inst);
#else

		snprintf(prefix, sizeof(prefix), "wan0_");

		/* check IP and PPP for primary wan */
		check_ip_ppp_ethernet("wan0_", pri_wan_inst);
		wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

		/* set Device.Ethernet.InterfaceNumberOfEntries as 2 (wan + lan)*/
		__set_parameter_value(ETH_IF_NUM, "2");	

		/* set static inform for IPAddress of IP */
		if (!strcmp(wan_proto, "dhcp") || !strcmp(wan_proto, "static")) {
			snprintf(buf, sizeof(buf), "%s.%d", ETH_IF, pri_wan_inst);
			if ((ip_if_inst = search_ip_ppp_lowerlayer(buf, IP_FLAG))) {
				snprintf(buf, sizeof(buf), "%s.%d.IPv4Address.1.IPAddress", IP_IF, ip_if_inst);
				add_inform_parameter(buf, 0);
			}
		}
		else
		{
			snprintf(buf, sizeof(buf), "%s.%d", ETH_IF, pri_wan_inst);
			if ((ppp_if_inst = search_ip_ppp_lowerlayer(buf, PPP_FLAG))) {
				snprintf(buf, sizeof(buf), "%s.%d", PPP_IF, ppp_if_inst);
				if ((ip_if_inst = search_ip_ppp_lowerlayer(buf, IP_FLAG))) {
					snprintf(buf, sizeof(buf), "%s.%d.IPv4Address.1.IPAddress", IP_IF, ip_if_inst);
					add_inform_parameter(buf, 0);
				}
			}
		}
#endif		
	}

	}	/*if (sw_mode == SW_MODE_ROUTER) */
	else if (sw_mode == SW_MODE_AP)
	{
		if (sw_mode != tr_mode)	/* reset interface */
			reset_interface();

		/* check and create Ethernet.Interface.X */
		__get_parameter_value(ETH_IF_NUM, eth_if_num_str);
		if (atoi(eth_if_num_str) == 0 || atoi(eth_if_num_str) != eth_if_num) {
			snprintf(buf, sizeof(buf), "%s", ETH_IF);
			if (delete_all_instance(buf))
				tr_log(LOG_ERROR, "delete_all_instance failed: %s", buf);

			snprintf(buf, sizeof(buf), "%s.", ETH_IF);
			for (i = 0; i < eth_if_num; i++) {
				if (add_object(buf, strlen(buf)) >= 9000) {
					tr_log(LOG_ERROR, "add_object failed: %s", buf);
					return -1;
				}
			}

			snprintf(eth_if_num_str, sizeof(eth_if_num_str), "%d", eth_if_num);
			__set_parameter_value(ETH_IF_NUM, eth_if_num_str);			
		}
	}

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

int del_wifi_ssid_ap(char *path)
{
	char buf[128] = {0};
	node_t node;	

	if (lib_resolve_node(path, &node) == 0) {
		snprintf(buf, sizeof(buf), "%s.", path);
		if (delete_object(buf, strlen(buf)) != 0) {
			tr_log(LOG_ERROR, "%s - delete_object failed: %s", __FUNCTION__, buf);
			return -1;
		}
	}

	return 0;
}

int update_wifi_radio_ssid_ap()
{
	int radio_num = 0;
	char buf[128] = {0};
	int ssid_count = 0;
#ifdef ASUSWRT
	int mssid_2g = num_of_mssid_support(0);
	int mssid_5g = num_of_mssid_support(1);
#else 	/* DSL_ASUSWRT */
	int mssid_2g = NUM_MSSID_SUPPORT;
	int mssid_5g = NUM_MSSID_SUPPORT;	
#endif
	int support_2g = nvram_contains_word("rc_support", "2.4G") ? 1 : 0;
	int support_5g = nvram_contains_word("rc_support", "5G") ? 1 : 0;
	char radio_num_str[4] = {0}, ssid_ap_num_str[4];
	int i = 0;

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	radio_num = support_2g + support_5g;
	ssid_count = radio_num + mssid_2g + mssid_5g;
	
	/* update Device.Wifi.RadioNumberOfEntries */
	snprintf(radio_num_str, sizeof(radio_num_str), "%d", radio_num);
	snprintf(buf, sizeof(buf), "%s.RadioNumberOfEntries", RADIO);
	__set_parameter_value(buf, radio_num_str);

	/* check radio, ssid and ap for 2G support */
	if (support_2g) {	/* 2G support */
		for (i = (mssid_2g + 2); i <= 4; i++) {
			/* delete redundent SSID.X for 2G */
			snprintf(buf, sizeof(buf), "%s.%d", WIFI_SSID, i);
			del_wifi_ssid_ap(buf);

			/* delete redundent AccessPoint.X for 2G */
			snprintf(buf, sizeof(buf), "%s.%d", WIFI_AP, i);
			del_wifi_ssid_ap(buf);
		}	
	}
	else 	/* 2G doesn't support */
	{
		for (i = 1; i <= 4; i++) {
			/* delete redundent SSID.X for 2G */
			snprintf(buf, sizeof(buf), "%s.%d", WIFI_SSID, i);
			del_wifi_ssid_ap(buf);

			/* delete redundent AccessPoint.X for 2G */
			snprintf(buf, sizeof(buf), "%s.%d", WIFI_AP, i);
			del_wifi_ssid_ap(buf);
		}
	}

	/* check radio, ssid and ap for 5G support */
	if (support_5g) {	/* 5G support */
		for (i = (mssid_5g + 6); i <= 8; i++) {
			/* delete redundent SSID.X for 2G */
			snprintf(buf, sizeof(buf), "%s.%d", WIFI_SSID, i);
			del_wifi_ssid_ap(buf);

			/* delete redundent AccessPoint.X for 2G */
			snprintf(buf, sizeof(buf), "%s.%d", WIFI_AP, i);
			del_wifi_ssid_ap(buf);
		}	
	}
	else 	/* 5G doesn't support */
	{
		for (i = 5; i <= 8; i++) {
			/* delete redundent SSID.X for 2G */
			snprintf(buf, sizeof(buf), "%s.%d", WIFI_SSID, i);
			del_wifi_ssid_ap(buf);

			/* delete redundent AccessPoint.X for 2G */
			snprintf(buf, sizeof(buf), "%s.%d", WIFI_AP, i);
			del_wifi_ssid_ap(buf);
		}
	}

	/* update Device.Wifi.SSIDNumberOfEntries */
	snprintf(ssid_ap_num_str, sizeof(ssid_ap_num_str), "%d", ssid_count);
	snprintf(buf, sizeof(buf), "%s.SSIDNumberOfEntries", RADIO);
	__set_parameter_value(buf, ssid_ap_num_str);	

	/* update Device.Wifi.AccessPointNumberOfEntries */
	snprintf(buf, sizeof(buf), "%s.AccessPointNumberOfEntries", RADIO);
	__set_parameter_value(buf, ssid_ap_num_str);	
	
	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;
}

int update_bridging()
{
	char buf[256] = {0}, lowerlayer[64] = {0}, mng_port_lowerlayers[512] = {0};
	int eth_lan_index = 0, wifi_ssid_index = 0;
	int i = 0, bridge_port_num = 0;
	int radio_num = 0, ssid_count = 0;
	int ip_if_index = 0;
#ifdef ASUSWRT
	int mssid_2g = num_of_mssid_support(0);
	int mssid_5g = num_of_mssid_support(1);
#else 	/* DSL_ASUSWRT */
	int mssid_2g = NUM_MSSID_SUPPORT;
	int mssid_5g = NUM_MSSID_SUPPORT;	
#endif
	int support_2g = nvram_contains_word("rc_support", "2.4G") ? 1 : 0;
	int support_5g = nvram_contains_word("rc_support", "5G") ? 1 : 0;
	
	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	radio_num = support_2g + support_5g;
	ssid_count = radio_num + mssid_2g + mssid_5g;

	memset(mng_port_lowerlayers, 0, sizeof(mng_port_lowerlayers));

	/* delete all instance of Device.Bridging.Bridge.1.Port and re-build */
	snprintf(buf, sizeof(buf), "%s", DEVICE_BRIDGING_BRIDGE_PORT_ONE);
	if (delete_all_instance(buf)) {
		tr_log(LOG_ERROR, "delete_all_instance failed: %s", buf);
		return -1;
	}

	/* create the instance of Device.Bridging.Bridge.1.Port.X for management port */
	snprintf(buf, sizeof(buf), "%s.", DEVICE_BRIDGING_BRIDGE_PORT_ONE);	/* Device.Bridging.Bridge.1.Port.1 */
	if ((mng_port_index = add_object(buf, strlen(buf))) >= 9000) {
		tr_log(LOG_ERROR, "add_object failed: %s", buf);
		return -1;
	}
	else
		bridge_port_num++;

	/* set ManagementPort of Device.Bridging.Bridge.1.Port.X (mng_port_index) */ 
	snprintf(buf, sizeof(buf), "%s.%d.ManagementPort", DEVICE_BRIDGING_BRIDGE_PORT_ONE, mng_port_index);
	__set_parameter_value(buf, "true");

	/* check IP.Interface.X.LowerLayers for management port */
	snprintf(buf, sizeof(buf), "%s.%d", DEVICE_BRIDGING_BRIDGE_PORT_ONE, mng_port_index);
	if ((ip_if_index = search_ip_ppp_lowerlayer(buf, IP_FLAG)) == 0) {	/* no ip lowerlayer refer to management port */
		/* create the instance of Device.IP.Interface.X */
		snprintf(buf, sizeof(buf), "%s.", IP_IF);
		if ((ip_if_index = add_object(buf, strlen(buf))) >= 9000) {
			tr_log(LOG_ERROR, "add_object failed: %s", buf);
			return -1;
		}

		/* add Device.IP.Interface.X (ip_if_index) to mng_port_lowerlayers */
		snprintf(buf, sizeof(buf), "%s.%d.LowerLayers", IP_IF, ip_if_index);
		snprintf(lowerlayer, sizeof(lowerlayer), "%s.%d", DEVICE_BRIDGING_BRIDGE_PORT_ONE, mng_port_index);
		__set_parameter_value(buf, lowerlayer);

	}

	/* create the instance of Device.Bridging.Bridge.1.Port.X for Ethernet.Interface for LAN and set lowerlayers */
	snprintf(buf, sizeof(buf), "%s.", DEVICE_BRIDGING_BRIDGE_PORT_ONE);	/* Device.Bridging.Bridge.1.Port.X */
	if ((eth_lan_index = add_object(buf, strlen(buf))) >= 9000) {
		tr_log(LOG_ERROR, "add_object failed: %s", buf);
		return -1;
	}
	else
		bridge_port_num++;

	/* set lowerlayers of Device.Bridging.Bridge.1.Port.X (eth_lan_index) */ 
	snprintf(buf, sizeof(buf), "%s.%d.LowerLayers", DEVICE_BRIDGING_BRIDGE_PORT_ONE, eth_lan_index);
	snprintf(lowerlayer, sizeof(lowerlayer), "%s.%d", ETH_IF, DEVICE_ETH_LAN_INST);
	__set_parameter_value(buf, lowerlayer);

	/* add Device.Bridging.Bridge.1.Port.X (eth_lan_index) to mng_port_lowerlayers */
	snprintf(buf, sizeof(buf), "%s.%d", DEVICE_BRIDGING_BRIDGE_PORT_ONE, eth_lan_index);
	sprintf(mng_port_lowerlayers + strlen(mng_port_lowerlayers), "%s%s", strlen(mng_port_lowerlayers) ? "," : "", buf);

	/* create port for 2.4G */
	if (support_2g) {	/* 2.4G support */
		for (i = START_INDEX_2G; i <= (mssid_2g + 1); i++) {
			/* create the instance of Device.Bridging.Bridge.1.Port.X for WiFi.SSID and set lowerlayers */
			snprintf(buf, sizeof(buf), "%s.", DEVICE_BRIDGING_BRIDGE_PORT_ONE);	/* Device.Bridging.Bridge.1.Port.X */
			if ((wifi_ssid_index = add_object(buf, strlen(buf))) >= 9000) {
				tr_log(LOG_ERROR, "add_object failed: %s", buf);
				return -1;
			}
			else
				bridge_port_num++;

			/* set lowerlayers of Device.Bridging.Bridge.1.Port.X (wifi_ssid_index) */ 
			snprintf(buf, sizeof(buf), "%s.%d.LowerLayers", DEVICE_BRIDGING_BRIDGE_PORT_ONE, wifi_ssid_index);
			snprintf(lowerlayer, sizeof(lowerlayer), "%s.%d", WIFI_SSID, i);
			__set_parameter_value(buf, lowerlayer);

			/* add Device.Bridging.Bridge.1.Port.X (wifi_ssid_index) to mng_port_lowerlayers */
			snprintf(buf, sizeof(buf), "%s.%d", DEVICE_BRIDGING_BRIDGE_PORT_ONE, wifi_ssid_index);
			sprintf(mng_port_lowerlayers + strlen(mng_port_lowerlayers), "%s%s", strlen(mng_port_lowerlayers) ? "," : "", buf);
		}	
	}


	/* create port for 5G */
	if (support_5g) {	/* 5G support */
		for (i = START_INDEX_5G; i <= (START_INDEX_5G + mssid_5g); i++) {
			/* create the instance of Device.Bridging.Bridge.1.Port.X for WiFi.SSID and set lowerlayers */
			snprintf(buf, sizeof(buf), "%s.", DEVICE_BRIDGING_BRIDGE_PORT_ONE);	/* Device.Bridging.Bridge.1.Port.X */
			if ((wifi_ssid_index = add_object(buf, strlen(buf))) >= 9000) {
				tr_log(LOG_ERROR, "add_object failed: %s", buf);
				return -1;
			}
			else
				bridge_port_num++;

			/* set lowerlayers of Device.Bridging.Bridge.1.Port.X (wifi_ssid_index) */ 
			snprintf(buf, sizeof(buf), "%s.%d.LowerLayers", DEVICE_BRIDGING_BRIDGE_PORT_ONE, wifi_ssid_index);
			snprintf(lowerlayer, sizeof(lowerlayer), "%s.%d", WIFI_SSID, i);
			__set_parameter_value(buf, lowerlayer);

			/* add Device.Bridging.Bridge.1.Port.X (wifi_ssid_index) to mng_port_lowerlayers */
			snprintf(buf, sizeof(buf), "%s.%d", DEVICE_BRIDGING_BRIDGE_PORT_ONE, wifi_ssid_index);
			sprintf(mng_port_lowerlayers + strlen(mng_port_lowerlayers), "%s%s", strlen(mng_port_lowerlayers) ? "," : "", buf);
		}	
	}

	/* set lowerlayers info of Device.Bridging.Bridge.1.Port.1.Lowerlayers (mng_port_index) */
	snprintf(buf, sizeof(buf), "%s.%d.LowerLayers", DEVICE_BRIDGING_BRIDGE_PORT_ONE, mng_port_index);
	__set_parameter_value(buf, mng_port_lowerlayers);


	if (sw_mode == SW_MODE_AP) {
		snprintf(buf, sizeof(buf), "%s.%d.IPv4Address.1.IPAddress", IP_IF, ip_if_index);
		add_inform_parameter(buf, 0);
	}

	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);

	return 0;	
}

static int add_ip_if(node_t node, char *arg, int nin)
{
	/* update InterfaceNumberOfEntries for Device.IP */
	update_if_num(DEVICE_IP, "InterfaceNumberOfEntries");

	return 0;
}

static int del_ip_if(node_t node, char *arg, int nin)
{
	/* update InterfaceNumberOfEntries for Device.PPP */
	update_if_num(DEVICE_IP, "InterfaceNumberOfEntries");

	return 0;
}

static int set_ip_if_lowerlayers(node_t node, char *arg, char *value)
{
	char tmp[32] = {0}, prefix[] = "wanXXXXXXXXXX_";
	int res = -1;

	memset(prefix, 0, sizeof(prefix));
	snprintf(prefix, sizeof(prefix), "%s", ethernet_prefix_by_path(value, tmp));

	if (strlen(prefix) == 0)
		return -1;

#ifdef ASUSWRT
	if (strstr(value, ETH_IF))
		res = set_nvram(strcat_r(prefix, "proto", tmp), "dhcp");
#ifdef RTCONFIG_DSL
	else if (strstr(value, ATM_LINK))
		res = set_nvram(strcat_r(prefix, "proto", tmp), "ipoa");
#endif
#ifdef RTCONFIG_VDSL
	else if (strstr(value, PTM_LINK))
		res = set_nvram(strcat_r(prefix, "proto", tmp), "dhcp");
#endif

#else 	/* DSL_ASUSWRT */
	if (strstr(value, ETH_IF)
#ifdef TCSUPPORT_WAN_ATM
		|| strstr(value, ATM_LINK)
#endif
#ifdef TCSUPPORT_WAN_PTM
		|| strstr(value, PTM_LINK)
#endif
	)
		res = set_nvram(convert_prefix_to_pvc(prefix, tmp), "ISP", "0");	/* dhcp */
#endif

	return res;
}

static int add_ppp_if(node_t node, char *arg, int nin)
{
	/* update InterfaceNumberOfEntries for Device.PPP */
	update_if_num(DEVICE_PPP, "InterfaceNumberOfEntries");

	return 0;
}

static int del_ppp_if(node_t node, char *arg, int nin)
{
	char tmp[32] = {0}, prefix[] = "wanXXXXXXXXXX_", buf[256] = {0};
	char lowerlayers[64] = {0};
	char *path = lib_node2path(node);
#ifdef ASUSWRT
	char *wan_proto = NULL;
#else 	/* DSL_ASUSWRT */
	char wan_proto[8] = {0};
#endif

	path[strlen(path) - 1] = '\0';

	tr_log(LOG_DEBUG, "%s - path=%s, nin=%d", __FUNCTION__, path, nin);

	memset(prefix, 0, sizeof(prefix));
	snprintf(prefix, sizeof(prefix), "%s", ethernet_prefix_by_path(path, tmp));
	if (strlen(prefix) == 0) {
		/* update InterfaceNumberOfEntries for Device.PPP */
		update_if_num(DEVICE_PPP, "InterfaceNumberOfEntries");
		return 0;
	}

	/* check LowerLayers */
	snprintf(buf, sizeof(buf), "%s.%d.LowerLayers", path, nin);
	__get_parameter_value(buf, lowerlayers);

#ifdef ASUSWRT
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	if (strstr(lowerlayers, ETH_IF)) {
		if (!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp"))
			set_nvram(strcat_r(prefix, "proto", tmp), "dhcp");
	}
#ifdef RTCONFIG_DSL
	else if (strstr(lowerlayers, ATM_LINK)) {
		if (!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "pppoa"))
			set_nvram(strcat_r(prefix, "proto", tmp), "dhcp");
	}
#endif
#ifdef RTCONFIG_VDSL
	else if (strstr(lowerlayers, PTM_LINK)) {
		if (!strcmp(wan_proto, "pppoe"))
			set_nvram(strcat_r(prefix, "proto", tmp), "dhcp");		
	}
#endif

#else 	/* DSL_ASUSWRT */
	tcapi_get(WANDUCK_NODE, strcat_r(prefix, "proto", tmp), wan_proto);
	if (strstr(lowerlayers, ETH_IF)
#ifdef TCSUPPORT_WAN_ATM
		|| strstr(lowerlayers, ATM_LINK)
#endif
#ifdef TCSUPPORT_WAN_PTM
		|| strstr(lowerlayers, PTM_LINK)
#endif
	) {
		if (!strcmp(wan_proto, "pppoe"))
			set_nvram(convert_prefix_to_pvc(prefix, tmp), "ISP", "0");	/* dhcp */
	}
#endif

	/* update InterfaceNumberOfEntries for Device.PPP */
	update_if_num(DEVICE_PPP, "InterfaceNumberOfEntries");

	return 0;
}

static int set_ppp_if_lowerlayers(node_t node, char *arg, char *value)
{
	char tmp[32] = {0}, prefix[] = "wanXXXXXXXXXX_";
	int res = 0;

	memset(prefix, 0, sizeof(prefix));
	snprintf(prefix, sizeof(prefix), "%s", ethernet_prefix_by_path(value, tmp));
	if (strlen(prefix) == 0)
		return 0;

#ifdef ASUSWRT
	if (strstr(value, ETH_IF))
		res = set_nvram(strcat_r(prefix, "proto", tmp), "pppoe");
#ifdef RTCONFIG_DSL
	else if (strstr(value, ATM_LINK))
		res = set_nvram(strcat_r(prefix, "proto", tmp), "pppoe");
#endif
#ifdef RTCONFIG_VDSL
	else if (strstr(value, PTM_LINK))
		res = set_nvram(strcat_r(prefix, "proto", tmp), "pppoe");		
#endif

#else 	/* DSL_ASUSWRT */
	if (strstr(value, ETH_IF)
#ifdef TCSUPPORT_WAN_ATM
		|| strstr(value, ATM_LINK)
#endif
#ifdef TCSUPPORT_WAN_PTM
		|| strstr(value, PTM_LINK)
#endif
	)
		res = set_nvram(convert_prefix_to_pvc(prefix, tmp), "ISP", "2");	/* pppoe */	
#endif

	return res;
}
#endif	/* end of TR181 */


/* ASUS specific */ 
static int get_cpuusage(node_t node, char *arg, char **value)
{
	char buf[32] = {0};

	//tr_log(LOG_DEBUG, "cpu_stat1: %u %u %u %u", cpu_stat1.user, cpu_stat1.nice, cpu_stat1.system, cpu_stat1.idle);
	get_cpuoccupy((CPU_OCCUPY * )&cpu_stat2);
	//tr_log(LOG_DEBUG, "cpu_stat2: %u %u %u %u", cpu_stat2.user, cpu_stat2.nice, cpu_stat2.system, cpu_stat2.idle);
	snprintf(buf, sizeof(buf), "%d", cal_cpuoccupy ((CPU_OCCUPY *)&cpu_stat1, (CPU_OCCUPY *)&cpu_stat2));
	get_cpuoccupy((CPU_OCCUPY * )&cpu_stat1);

    *value = strdup(buf);
    return *value ? 0 : -1;
}

int get_wan_prefix_ifunit(node_t node)
{
#ifdef RTCONFIG_DUALWAN
	char tmp[64] = {0};
	char *priWan = NULL, *secWan = NULL;
#endif
	int unit = -1;
	char *path = lib_node2path(node);

#ifdef RTCONFIG_DUALWAN
	memset(tmp, 0, sizeof(tmp));
#ifdef ASUSWRT
	sprintf(tmp, "%s", nvram_safe_get("wans_dualwan"));
#else 	/* DSL_ASUSWRT */
	tcapi_get(DUALWAN_NODE, "wans_dualwan", tmp);
#endif
	if(vstrsep(tmp, " ", &priWan, &secWan) != 2)
		return -1;
#endif

#ifdef ASUSWRT
#ifdef RTCONFIG_XDSL
	if (strstr(path, "X_ASUS_DslType")) {
#ifdef RTCONFIG_DUALWAN
		if(!strcmp(priWan, "dsl"))
			unit = 0;
		else if(!strcmp(secWan, "dsl"))
			unit = 1;
#else /* !RTCONFIG_DUALWAN */
		unit = 0;
#endif
	}
#else
	if (strstr(path, "X_ASUS_WanType")) {
#ifdef RTCONFIG_DUALWAN
		if(!strcmp(priWan, "wan"))
			unit = 0;
		else if(!strcmp(secWan, "wan"))
			unit = 1;
#else /* !RTCONFIG_DUALWAN */
		unit = 0;
#endif
	}
#endif
#ifdef RTCONFIG_DUALWAN
	else if (strstr(path, "X_ASUS_LanType")) {
		if(!strcmp(priWan, "lan"))
			unit = 0;
		else if(!strcmp(secWan, "lan"))
			unit = 1;
	}
#endif
#ifdef RTCONFIG_USB_MODEM
	else if (strstr(path, "X_ASUS_UsbType")) {
#ifdef RTCONFIG_DUALWAN
		if(!strcmp(priWan, "usb"))
			unit = 0;
		else if(!strcmp(secWan, "usb"))
			unit = 1;
#else /* !RTCONFIG_DUALWAN */
		unit = 1;
#endif
	}
#endif

#else 	/* DSL_ASUSWRT */

#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	if (strstr(path, "X_ASUS_DslType"))
#ifdef TR098
		unit = convert_wd_inst_to_wan_unit(IGD_WANDEVICE_DSL_INST);		
#endif
#ifdef TR181
		unit = convert_eth_inst_to_wan_unit(DEVICE_DSL_INST);		
#endif
#endif

#ifdef TCSUPPORT_WAN_ETHER
	if (strstr(path, "X_ASUS_WanType"))
#ifdef TR098		
		unit = convert_wd_inst_to_wan_unit(IGD_WANDEVICE_WAN_INST);
#endif
#ifdef TR181
		unit = convert_eth_inst_to_wan_unit(DEVICE_ETH_WAN_INST);		
#endif
#endif

#ifdef RTCONFIG_USB_MODEM
	if (strstr(path, "X_ASUS_UsbType"))
#ifdef TR098
		unit = convert_wd_inst_to_wan_unit(IGD_WANDEVICE_USB_INST);	
#endif
#ifdef TR181
		unit = convert_eth_inst_to_wan_unit(DEVICE_USB_INST);		
#endif
#endif
#endif

	return unit;
}

#ifdef ASUSWRT
static int set_wan_connectiontype(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")], tmp[32];
	int res = 0;
	int unit = get_wan_prefix_ifunit(node);

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	res = nvram_set(strcat_r(prefix, "proto", tmp), value);

	return res;
}

static int get_wan_connectiontype(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")], tmp[32];
	char buf[64];
	int unit = get_wan_prefix_ifunit(node);

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "proto", tmp)));
	*value = strdup(buf);
	return *value ? 0 : -1;	
}

static int set_wan_dhcpenable(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")], tmp[32];
	int res = 0;
	int unit = get_wan_prefix_ifunit(node);
	char *value_conv = NULL;

	if (unit == -1)
		return -1;

	value_conv = (!war_strcasecmp(value, "1") || !war_strcasecmp(value, "true")) ? "1" : "0";
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	res = set_nvram(strcat_r(prefix, "dhcpenable_x", tmp), value_conv);

	return res;
}

static int get_wan_dhcpenable(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")], tmp[32];
	char buf[64];
	int unit = get_wan_prefix_ifunit(node);

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	snprintf(buf, sizeof(buf), "%s", (atoi(nvram_safe_get(strcat_r(prefix, "dhcpenable_x", tmp))) == 1) ? "true": "false");

	*value = strdup(buf);
	return *value ? 0 : -1;	
}

static int set_wan_ip(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")], tmp[32];
	int res = 0;
	int unit = get_wan_prefix_ifunit(node);
	char *wan_proto = NULL, *wan_dhcpenable = NULL;

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp")) {
		wan_dhcpenable = nvram_safe_get(strcat_r(prefix, "dhcpenable_x", tmp));
		if(!strcmp(wan_dhcpenable, "1"))	/* can't be modified */
			return -1;
		else
			res = set_nvram(strcat_r(prefix, "ipaddr_x", tmp), value);
	}
	else
	{
		if(!strcmp(wan_proto, "dhcp"))	/* can't be modified */
			return -1;
		else
			res = set_nvram(strcat_r(prefix, "ipaddr_x", tmp), value);
	}	

	return res;
}

static int get_wan_ip(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")], tmp[32];
	char buf[64];
	int unit = get_wan_prefix_ifunit(node);
	char *wan_proto = NULL, *wan_dhcpenable = NULL;

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp") ) {
		wan_dhcpenable = nvram_safe_get(strcat_r(prefix, "dhcpenable_x", tmp));
		snprintf(buf, sizeof(buf), "%s", !strcmp(wan_dhcpenable, "1") ? 
						nvram_safe_get(strcat_r(prefix, "xipaddr", tmp)) : 
						nvram_safe_get(strcat_r(prefix, "ipaddr_x", tmp)));
	}
	else
		snprintf(buf, sizeof(buf), "%s", !strcmp(wan_proto, "dhcp") ? 
						nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)) : 
						nvram_safe_get(strcat_r(prefix, "ipaddr_x", tmp)));

	*value = strdup(buf);
	return *value ? 0 : -1;	
}

static int set_wan_netmask(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")], tmp[32];
	int res = 0;
	int unit = get_wan_prefix_ifunit(node);
	char *wan_proto = NULL, *wan_dhcpenable = NULL;

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp")) {
		wan_dhcpenable = nvram_safe_get(strcat_r(prefix, "dhcpenable_x", tmp));
		if(!strcmp(wan_dhcpenable, "1"))	/* can't be modified */
			return -1;
		else
			res = set_nvram(strcat_r(prefix, "netmask_x", tmp), value);
	}
	else
	{
		if(!strcmp(wan_proto, "dhcp"))	/* can't be modified */
			return -1;
		else
			res = set_nvram(strcat_r(prefix, "netmask_x", tmp), value);
	}

	return res;
}

static int get_wan_netmask(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")], tmp[32];
	char buf[64];
	int unit = get_wan_prefix_ifunit(node);
	char *wan_proto = NULL, *wan_dhcpenable = NULL;

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	
	if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp") ) {
		wan_dhcpenable = nvram_safe_get(strcat_r(prefix, "dhcpenable_x", tmp));
		snprintf(buf, sizeof(buf), "%s", !strcmp(wan_dhcpenable, "1") ? 
						nvram_safe_get(strcat_r(prefix, "xnetmask", tmp)) : 
						nvram_safe_get(strcat_r(prefix, "netmask_x", tmp)));
	}
	else
		snprintf(buf, sizeof(buf), "%s", !strcmp(wan_proto, "dhcp") ? 
						nvram_safe_get(strcat_r(prefix, "netmask", tmp)) : 
						nvram_safe_get(strcat_r(prefix, "netmask_x", tmp)));
	
	*value = strdup(buf);
	return *value ? 0 : -1;	
}

static int set_wan_gateway(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")], tmp[32];
	int res = 0;
	int unit = get_wan_prefix_ifunit(node);
	char *wan_proto = NULL, *wan_dhcpenable = NULL;

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp")) {
		wan_dhcpenable = nvram_safe_get(strcat_r(prefix, "dhcpenable_x", tmp));
		if(!strcmp(wan_dhcpenable, "1"))	/* can't be modified */
			return -1;
		else
			res = set_nvram(strcat_r(prefix, "gateway_x", tmp), value);
	}
	else
	{
		if(!strcmp(wan_proto, "dhcp"))	/* can't be modified */
			return -1;
		else
			res = set_nvram(strcat_r(prefix, "gateway_x", tmp), value);
	}

	return res;
}

static int get_wan_gateway(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")], tmp[32];
	char buf[64];
	int unit = get_wan_prefix_ifunit(node);
	char *wan_proto = NULL, *wan_dhcpenable = NULL;

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	
	if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp") ) {
		wan_dhcpenable = nvram_safe_get(strcat_r(prefix, "dhcpenable_x", tmp));
		snprintf(buf, sizeof(buf), "%s", !strcmp(wan_dhcpenable, "1") ? 
						nvram_safe_get(strcat_r(prefix, "xgateway", tmp)) : 
						nvram_safe_get(strcat_r(prefix, "gateway_x", tmp)));
	}
	else
		snprintf(buf, sizeof(buf), "%s", !strcmp(wan_proto, "dhcp") ? 
						nvram_safe_get(strcat_r(prefix, "gateway", tmp)) : 
						nvram_safe_get(strcat_r(prefix, "gateway_x", tmp)));

	*value = strdup(buf);
	return *value ? 0 : -1;	
}

static int set_wan_dnsenable(node_t node, char *arg, char *value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")], tmp[32];
	int res = 0;
	int unit = get_wan_prefix_ifunit(node);
	char *wan_proto = NULL, *value_conv = NULL;

	if (unit == -1)
		return -1;

	value_conv = (!war_strcasecmp(value, "1") || !war_strcasecmp(value, "true")) ? "1" : "0";
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp"))	
		res = set_nvram(strcat_r(prefix, "dnsenable_x", tmp), value_conv);
	else
	{
		if(!strcmp(wan_proto, "dhcp"))
			res = set_nvram(strcat_r(prefix, "dnsenable_x", tmp), value_conv);
		else		/* can't be modified */
			return -1;
	}

	return res;
}

static int get_wan_dnsenable(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")], tmp[32];
	char buf[64];
	int unit = get_wan_prefix_ifunit(node);
	char *wan_proto = NULL;

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	
	if(!strcmp(wan_proto, "pppoe") || !strcmp(wan_proto, "l2tp") || !strcmp(wan_proto, "pptp") )
		snprintf(buf, sizeof(buf), "%s", (atoi(nvram_safe_get(strcat_r(prefix, "dnsenable_x", tmp))) == 1) ? "true": "false");
	else
	{
		if(!strcmp(wan_proto, "dhcp"))
			snprintf(buf, sizeof(buf), "%s", (atoi(nvram_safe_get(strcat_r(prefix, "dnsenable_x", tmp))) == 1) ? "true": "false");
		else
			snprintf(buf, sizeof(buf), "true");
	}

	*value = strdup(buf);
	return *value ? 0 : -1;	
}

static int set_wan_dns1(node_t node, char *arg, char *value)
{
	char buf[32], prefix[sizeof("wanXXXXXXXXXX_")];
	int res = 0;
	int unit = get_wan_prefix_ifunit(node);

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	sprintf(buf, "%sdns1_x", prefix);
	res = nvram_set(buf, value);

	return res;
}

static int get_wan_dns1(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")], tmp[32];
	char buf[64];
	int unit = get_wan_prefix_ifunit(node);

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "dns1_x", tmp)));
	*value = strdup(buf);
	return *value ? 0 : -1;	
}

static int set_wan_dns2(node_t node, char *arg, char *value)
{
	char buf[32], prefix[sizeof("wanXXXXXXXXXX_")];
	int res = 0;
	int unit = get_wan_prefix_ifunit(node);

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	sprintf(buf, "%sdns2_x", prefix);
	res = nvram_set(buf, value);

	return res;
}

static int get_wan_dns2(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")], tmp[32];
	char buf[64];
	int unit = get_wan_prefix_ifunit(node);

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "dns2_x", tmp)));
	*value = strdup(buf);
	return *value ? 0 : -1;	
}

static int set_wan_username(node_t node, char *arg, char *value)
{
	char buf[32], prefix[sizeof("wanXXXXXXXXXX_")];
	int res = 0;
	int unit = get_wan_prefix_ifunit(node);

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	sprintf(buf, "%spppoe_username", prefix);
	res = nvram_set(buf, value);

	return res;
}

static int get_wan_username(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")], tmp[32];
	char buf[64];
	int unit = get_wan_prefix_ifunit(node);

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "pppoe_username", tmp)));
	*value = strdup(buf);
	return *value ? 0 : -1;	
}

static int set_wan_password(node_t node, char *arg, char *value)
{
	char buf[32], prefix[sizeof("wanXXXXXXXXXX_")];
	int res = 0;
	int unit = get_wan_prefix_ifunit(node);

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	sprintf(buf, "%spppoe_passwd", prefix);
	res = nvram_set(buf, value);

	return res;
}

static int get_wan_password(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")], tmp[32];
	char buf[64];
	int unit = get_wan_prefix_ifunit(node);

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "pppoe_passwd", tmp)));
	*value = strdup(buf);
	return *value ? 0 : -1;	
}

static int set_wan_vpnserver(node_t node, char *arg, char *value)
{
	char buf[32], prefix[sizeof("wanXXXXXXXXXX_")];
	int res = 0;
	int unit = get_wan_prefix_ifunit(node);

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	sprintf(buf, "%sheartbeat_x", prefix);
	res = nvram_set(buf, value);

	return res;
}

static int get_wan_vpnserver(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")], tmp[32];
	char buf[64];
	int unit = get_wan_prefix_ifunit(node);

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "heartbeat_x", tmp)));
	*value = strdup(buf);
	return *value ? 0 : -1;	
}

static int set_wan_hostname(node_t node, char *arg, char *value)
{
	char buf[32], prefix[sizeof("wanXXXXXXXXXX_")];
	int res = 0;
	int unit = get_wan_prefix_ifunit(node);

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	sprintf(buf, "%shostname", prefix);
	res = nvram_set(buf, value);

	return res;
}

static int get_wan_hostname(node_t node, char *arg, char **value)
{
	char prefix[sizeof("wanXXXXXXXXXX_")], tmp[32];
	char buf[64];
	int unit = get_wan_prefix_ifunit(node);

	if (unit == -1)
		return -1;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);
	snprintf(buf, sizeof(buf), "%s", nvram_safe_get(strcat_r(prefix, "hostname", tmp)));
	*value = strdup(buf);
	return *value ? 0 : -1;	
}
#endif	/* ASUSWRT */

static int set_primarywan(node_t node, char *arg, char *value)
{
#ifdef RTCONFIG_DUALWAN
	char tmp[64] = {0}, dualwan_str[32] = {0};
	char *priWan = NULL, *secWan = NULL;
	int res = -1;

	if (
#ifdef ASUSWRT
#ifdef RTCONFIG_XDSL
		!strcasecmp(value, "dsl") 
#else
		!strcasecmp(value, "wan") 
#endif
	 	 && !strcasecmp(value, "lan")
#ifdef RTCONFIG_USB_MODEM
	 	 && !strcasecmp(value, "usb")
#endif

#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	 	!strcasecmp(value, "dsl")
#endif
#ifdef TCSUPPORT_WAN_ETHER
	 	&& !strcasecmp(value, "wan")  
#endif
#ifdef RTCONFIG_USB_MODEM
	 	&& !strcasecmp(value, "usb")
#endif
#endif
	 )
		return -1;

	memset(tmp, 0, sizeof(tmp));
#ifdef ASUSWRT
	sprintf(tmp, "%s", nvram_safe_get("wans_dualwan"));
#else 	/* DSL_ASUSWRT */
	tcapi_get(DUALWAN_NODE, "wans_dualwan", tmp);
#endif

	if(vstrsep(tmp, " ", &priWan, &secWan) != 2)
		return -1;

#ifdef ASUSWRT
#ifdef RTCONFIG_XDSL
	if (!strcasecmp(value, "dsl")) {
		if (!strcmp(secWan, "dsl"))
			sprintf(dualwan_str, "dsl usb");	
		else
			sprintf(dualwan_str, "dsl %s", secWan);	
		set_nvram("wanports", "0");		
	}
	else if (!strcasecmp(value, "lan")) {//LAN
		if (!strcmp(secWan, "lan")) {
			sprintf(dualwan_str, "lan usb");
		}
		else
			sprintf(dualwan_str, "lan %s", secWan);	
	}
#else	
	if (!strcasecmp(value, "wan")) {	//WAN
		if (!strcmp(secWan, "wan"))
			sprintf(dualwan_str, "wan usb");	
		else
			sprintf(dualwan_str, "wan %s", secWan);	
		set_nvram("wanports", "0");		
	}
	else if (!strcasecmp(value, "lan")) {//LAN
		if (!strcmp(secWan, "lan")) {
			sprintf(dualwan_str, "lan wan");
			//set_nvram("wanports", "4");//default lan4
			//set_nvram("wan1ports", "0");//default lan4
		}
		else
			sprintf(dualwan_str, "lan %s", secWan);	
	}
#endif
	else if (!strcasecmp(value, "usb")) {//USB
		if (!strcmp(secWan, "usb")) {
			sprintf(dualwan_str, "usb lan");	
			//set_nvram("wan1ports", "4");//default lan4
		}
		else
			sprintf(dualwan_str, "usb %s", secWan);	
	}

#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	if (!strcasecmp(value, "dsl")) {
		if (!strcmp(secWan, "dsl"))
			sprintf(dualwan_str, "dsl none");
		else
			sprintf(dualwan_str, "dsl %s", secWan);

#ifdef TR098
		snprintf(tmp, sizeof(tmp), "%d", convert_wd_inst_to_wan_unit(IGD_WANDEVICE_DSL_INST));
#endif
#ifdef TR181
		snprintf(tmp, sizeof(tmp), "%d", convert_eth_inst_to_wan_unit(DEVICE_DSL_INST));
#endif
		tcapi_set(WANDUCK_NODE, "wan_primary", tmp);
	}
#endif

#ifdef TCSUPPORT_WAN_ETHER
	if (!strcasecmp(value, "wan")) {	//WAN
		if (!strcmp(secWan, "wan"))
			sprintf(dualwan_str, "wan none");
		else
			sprintf(dualwan_str, "wan %s", secWan);

#ifdef TR098
		snprintf(tmp, sizeof(tmp), "%d", convert_wd_inst_to_wan_unit(IGD_WANDEVICE_WAN_INST));
#endif
#ifdef TR181
		snprintf(tmp, sizeof(tmp), "%d", convert_eth_inst_to_wan_unit(DEVICE_ETH_WAN_INST));
#endif
		tcapi_set(WANDUCK_NODE, "wan_primary", tmp);
	}
#endif

#ifdef RTCONFIG_USB_MODEM
	if (!strcasecmp(value, "usb")) {//USB
		if (!strcmp(secWan, "usb")) {
			sprintf(dualwan_str, "usb none");
		}
		else
			sprintf(dualwan_str, "usb %s", secWan);

#ifdef TR098
		snprintf(tmp, sizeof(tmp), "%d", convert_wd_inst_to_wan_unit(IGD_WANDEVICE_USB_INST));
#endif
#ifdef TR181
		snprintf(tmp, sizeof(tmp), "%d", convert_eth_inst_to_wan_unit(DEVICE_USB_INST));
#endif
		tcapi_set(WANDUCK_NODE, "wan_primary", tmp);
	}
#endif
#endif

#ifdef ASUSWRT
	res = set_nvram("wans_dualwan", dualwan_str);
#else 	/* DSL_ASUSWRT */
	res = set_nvram(DUALWAN_NODE, "wans_dualwan", dualwan_str);
#endif

	return res;	
#else /* !RTCONFIG_DUALWAN */
	return -1;
#endif
}

static int get_primarywan(node_t node, char *arg, char **value)
{
#ifdef RTCONFIG_DUALWAN
	char tmp[64] = {0}, buf[64] = {0};
	char *priWan = NULL, *secWan = NULL;
	
	memset(tmp, 0, sizeof(tmp));
#ifdef ASUSWRT
	sprintf(tmp, "%s", nvram_safe_get("wans_dualwan"));
#else 	/* DSL_ASUSWRT */
	tcapi_get(DUALWAN_NODE, "wans_dualwan", tmp);
#endif

	if(vstrsep(tmp, " ", &priWan, &secWan) != 2)
		return -1;

	snprintf(buf, sizeof(buf), "%s", priWan);
	*value = strdup(buf);
#else /* !RTCONFIG_DUALWAN */
	*value = strdup("wan");
#endif
	return *value ? 0 : -1;	
}

static int set_secondarywan(node_t node, char *arg, char *value)
{
#ifdef RTCONFIG_DUALWAN
	char tmp[64] = {0}, dualwan_str[32] = {0};
	char *priWan = NULL, *secWan = NULL;
	int res = -1;

	if (
#ifdef ASUSWRT
#ifdef RTCONFIG_XDSL
		!strcasecmp(value, "dsl") 
#else
		!strcasecmp(value, "wan") 
#endif
	 	 && !strcasecmp(value, "lan")
#ifdef RTCONFIG_USB
	 	 && !strcasecmp(value, "usb")
#endif

#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	 	!strcasecmp(value, "dsl")
#endif
#ifdef TCSUPPORT_WAN_ETHER
	 	&& !strcasecmp(value, "wan")  
#endif
#ifdef RTCONFIG_USB_MODEM
	 	&& !strcasecmp(value, "usb")
#endif
#endif
	 )
		return -1;

	memset(tmp, 0, sizeof(tmp));
#ifdef ASUSWRT
	sprintf(tmp, "%s", nvram_safe_get("wans_dualwan"));
#else 	/* DSL_ASUSWRT */
	tcapi_get(DUALWAN_NODE, "wans_dualwan", tmp);
#endif

	if(vstrsep(tmp, " ", &priWan, &secWan) != 2)
		return -1;

#ifdef ASUSWRT
#ifdef RTCONFIG_XDSL
	if (!strcasecmp(value, "dsl"))	/* dsl can't be secondary wan now*/
		return -1;

	if (!strcasecmp(value, "lan")) {//LAN
		if (!strcmp(priWan, "lan")) {
			sprintf(dualwan_str, "dsl lan");
			//set_nvram("wanports", "0");
		}
		else
			sprintf(dualwan_str, "%s lan", priWan);	
	}
#else
	if (!strcasecmp(value, "wan")) {	//WAN
		if(!strcmp(priWan, "wan"))
			sprintf(dualwan_str, "usb wan");	
		else
			sprintf(dualwan_str, "%s wan", priWan);	
		set_nvram("wan1ports", "0");
	}
	else if (!strcasecmp(value, "lan")) {//LAN
		if (!strcmp(priWan, "lan")) {
			sprintf(dualwan_str, "wan lan");
			//set_nvram("wanports", "0");
		}
		else
			sprintf(dualwan_str, "%s lan", priWan);	
	}
#endif
	else if (!strcasecmp(value, "usb")) {//USB
		if (!strcmp(priWan, "usb")) {
			sprintf(dualwan_str, "lan usb");	
			//set_nvram("wanports", "4");//default lan4
		}
		else
			sprintf(dualwan_str, "%s usb", priWan);
	}

#else 	/* DSL_ASUSWRT */
#if defined(TCSUPPORT_WAN_ATM) || defined(TCSUPPORT_WAN_PTM)
	if (!strcasecmp(value, "dsl")) {
		if (!strcmp(priWan, "dsl"))
			sprintf(dualwan_str, "none dsl");
		else
			sprintf(dualwan_str, "%s dsl", priWan);

#ifdef TR098
		snprintf(tmp, sizeof(tmp), "%d", convert_wd_inst_to_wan_unit(IGD_WANDEVICE_DSL_INST));
#endif
#ifdef TR181
		snprintf(tmp, sizeof(tmp), "%d", convert_eth_inst_to_wan_unit(DEVICE_DSL_INST));
#endif
		tcapi_set(WANDUCK_NODE, "wan_secondary", tmp);	
	}
#endif

#ifdef TCSUPPORT_WAN_ETHER
	if (!strcasecmp(value, "wan")) {	//WAN
		if (!strcmp(priWan, "wan"))
			sprintf(dualwan_str, "none wan");
		else
			sprintf(dualwan_str, "%s wan", priWan);

#ifdef TR098
		snprintf(tmp, sizeof(tmp), "%d", convert_wd_inst_to_wan_unit(IGD_WANDEVICE_WAN_INST));
#endif
#ifdef TR181
		snprintf(tmp, sizeof(tmp), "%d", convert_eth_inst_to_wan_unit(DEVICE_ETH_WAN_INST));
#endif
		tcapi_set(WANDUCK_NODE, "wan_secondary", tmp);	
	}
#endif

#ifdef RTCONFIG_USB_MODEM
	if (!strcasecmp(value, "usb")) {//USB
		if (!strcmp(priWan, "usb")) {
			sprintf(dualwan_str, "none usb");
		}
		else
			sprintf(dualwan_str, "%s usb", priWan);

#ifdef TR098
		snprintf(tmp, sizeof(tmp), "%d", convert_wd_inst_to_wan_unit(IGD_WANDEVICE_USB_INST));
#endif
#ifdef TR181
		snprintf(tmp, sizeof(tmp), "%d", convert_eth_inst_to_wan_unit(DEVICE_USB_INST));
#endif
		tcapi_set(WANDUCK_NODE, "wan_secondary", tmp);	
	}
#endif
#endif

#ifdef ASUSWRT
	res = set_nvram("wans_dualwan", dualwan_str);
#else 	/* DSL_ASUSWRT */
	res = set_nvram(DUALWAN_NODE, "wans_dualwan", dualwan_str);
#endif

	return res;	
#else /* !RTCONFIG_DUALWAN */
	return -1;
#endif
}

static int get_secondarywan(node_t node, char *arg, char **value)
{
#ifdef RTCONFIG_DUALWAN
	char tmp[64] = {0}, buf[64] = {0};
	char *priWan = NULL, *secWan = NULL;
	
	memset(tmp, 0, sizeof(tmp));
#ifdef ASUSWRT
	sprintf(tmp, "%s", nvram_safe_get("wans_dualwan"));
#else 	/* DSL_ASUSWRT */
	tcapi_get(DUALWAN_NODE, "wans_dualwan", tmp);
#endif

	if(vstrsep(tmp, " ", &priWan, &secWan) != 2)
		return -1;

	snprintf(buf, sizeof(buf), "%s", secWan);
	*value = strdup(buf);
#elif defined(RTCONFIG_USB_MODEM)
	*value = strdup("usb");
#else
	*value = strdup("none");
#endif
	return *value ? 0 : -1;	
}
/* end of ASUS specific */

void reasign_instance_name(node_t node, int del_nin, int count_inc)
{
	char *path = lib_node2path(node);
	char buf[256] = {0}, nin[8] = {0};
	node_t *children = NULL;
	int count = 0, count_r = 0;

	tr_log(LOG_DEBUG, "%s - start", __FUNCTION__);

	path[strlen(path) - 1] = '\0';
	count = count_r = lib_get_children(node, &children);

	tr_log(LOG_DEBUG, "%s - count %d, count_r %d", __FUNCTION__, count, count_r);
	while(count > 0) {
		char name[16];
		count--;

		if(lib_get_property(children[count], "name", name) == 0) {
			if(string_is_digits(name) == 1) {
				node_t node_tmp;

				if (atoi(name) < del_nin || del_nin == atoi(name))
					continue;

				snprintf(buf, sizeof(buf), "%s.%s", path, name);
				tr_log(LOG_DEBUG, "%s - buf (%s)", __FUNCTION__, buf);
				if(lib_resolve_node(buf, &node_tmp) == 0) {
					char name_str[32];
					snprintf(name_str, sizeof(name_str), "%d", atoi(name) - 1);
					snprintf(node_tmp->name, sizeof(node_tmp->name), "%s", name_str);
				}
			}
		}
	}

	tr_log(LOG_DEBUG, "%s - count_inc (%d), count_r (%d)", __FUNCTION__, count_inc, count_r);
	if (count_inc)
		snprintf(nin, sizeof(nin), "%d", count_r + 1);
	else
		snprintf(nin, sizeof(nin), "%d", count_r);
	lib_set_property(node, "nin", nin);	/* Reset nin property as count - 1 */

	if(children) {
		lib_destroy_children(children);
		children = NULL;
	}
	tr_log(LOG_DEBUG, "%s - end", __FUNCTION__);
}

#ifdef RTCONFIG_SFEXPRESS
int update_ovpnc()
{
	char buf[256];
	node_t node;
	char *nv, *nvp, *b;
	char *desc, *type, *index, *username, *password;
	int count = 0;
	node_t *children = NULL;

	tr_log(LOG_DEBUG, "update_ovpnc - start");

	if(lib_resolve_node(X_ASUS_OPENVPNCLIENT, &node) == 0) {
		count = lib_get_children(node, &children);
		/* No instance of openvpn client */
		nv = nvp = strdup(nvram_safe_get("vpnc_clientlist"));
		if (count == 0) {
				if (nv) {
					while ((b = strsep(&nvp, "<")) != NULL) {
						int i = 0;
						char index_str[8];

						if ((vstrsep(b, ">", &desc, &type, &index, &username, &password) != 5))
							continue;

						snprintf(buf, sizeof(buf), "%s.", X_ASUS_OPENVPNCLIENT);
						if((i = add_object(buf, strlen(buf))) >= 9000) {
							tr_log(LOG_ERROR, "add_object failed: %s", buf);
							free(nv);
							return -1;				
						}

						snprintf(buf, sizeof(buf), "%s.%d.X_ASUS_Index", X_ASUS_OPENVPNCLIENT, i);
						snprintf(index_str, sizeof(index_str), "%s", index);
						__set_parameter_value(buf, index_str);
						tr_log(LOG_DEBUG, "update_ovpnc: index %s", index);
					}
					free(nv);
				}
		}
		else 	/* Have openvpn client instance */
		{
			/* Add instance from nvram to X_ASUS_OpenVPNClient */
				if (nv) {
					while ((b = strsep(&nvp, "<")) != NULL) {
						node_t node_tmp;
						node_t *children_t = NULL;
						int need_add = 1;
						char index_str[8];

						if ((vstrsep(b, ">", &desc, &type, &index, &username, &password) != 5))
							continue;

						if (strcmp(type, "OpenVPN"))
							continue;

						snprintf(buf, sizeof(buf), "%s", X_ASUS_OPENVPNCLIENT);
						if(lib_resolve_node(buf, &node_tmp) == 0) {
							int count_t = lib_get_children(node_tmp, &children_t);

					    	while(count_t > 0) {
								char name[16];
								count_t--;

								if(lib_get_property(children_t[count_t], "name", name) == 0) {
										if(string_is_digits(name) == 1) {
											char tmp_s[32];

											snprintf(buf, sizeof(buf), "%s.%s.X_ASUS_Index", X_ASUS_OPENVPNCLIENT, name);
											snprintf(index_str, sizeof(index_str), "%s", __get_parameter_value(buf, tmp_s));
											if(!strcmp(index_str, index)) {
												need_add = 0;
												break;
											}
										}
								}
						}

							if(children_t) {
								lib_destroy_children(children_t);
								children_t = NULL;
							}
						}

						if (need_add) {
							int i = 0;
							snprintf(buf, sizeof(buf), "%s.", X_ASUS_OPENVPNCLIENT);
							if((i = add_object(buf, strlen(buf))) >= 9000) {
								tr_log(LOG_ERROR, "add_object failed: %s", buf);
								free(nv);
								return -1;				
							}

							snprintf(buf, sizeof(buf), "%s.%d.X_ASUS_Index", X_ASUS_OPENVPNCLIENT, i);
							snprintf(index_str, sizeof(index_str), "%s", index);
							__set_parameter_value(buf, index_str);
							tr_log(LOG_DEBUG, "update_ovpnc: index %s", index);
						}
					}
					free(nv);
					nv = NULL;

					/* delete X_ASUS_OpenVPNClient's instance based on nvram */
					while(count > 0) {
						char name[16];
						count--;

						if(lib_get_property(children[count], "name", name) == 0) {
							if(string_is_digits(name) == 1) {
								char tmp_s[32];
								char index_str[8];
								int need_del = 1;

								snprintf(buf, sizeof(buf), "%s.%s.X_ASUS_Index", X_ASUS_OPENVPNCLIENT, name);
								snprintf(index_str, sizeof(index_str), "%s", __get_parameter_value(buf, tmp_s));

								nv = nvp = strdup(nvram_safe_get("vpnc_clientlist"));
								if (nv) {
									while ((b = strsep(&nvp, "<")) != NULL) {
										
										if ((vstrsep(b, ">", &desc, &type, &index, &username, &password) != 5))
											continue;

										if (strcmp(type, "OpenVPN"))
											continue;

										if(!strcmp(index_str, index)) {
											need_del = 0;
											break;
										}
									}
									free(nv);
									nv = NULL;
								}

								if (need_del) {
									node_t node_tmp;

									snprintf(buf, sizeof(buf), "%s.%s.", X_ASUS_OPENVPNCLIENT, name);
									tr_log(LOG_DEBUG, "The path for delete: %s", buf);
									delete_object(buf, strlen(buf));

									snprintf(buf, sizeof(buf), "%s.", X_ASUS_OPENVPNCLIENT);	
									if(lib_resolve_node(buf, &node_tmp) == 0)
										reasign_instance_name(node_tmp, atoi(name), 1);
								}
							}
						}
					}

					free(nv);
				}
	    	}

	    	if(children) {
				lib_destroy_children(children);
				children = NULL;
			}
	}
	else
		return -1;
	
	tr_log(LOG_DEBUG, "update_ovpnc - end");

	return 0;
}

int get_ovpnc_index_value(node_t node)
{
	char *path = lib_node2path(node->parent);
	char buf[256] = {0}, index[8] = {0}, tmp[64] = {0};
	int value = 0;

	path[strlen(path) - 1] = '\0';

	snprintf(buf, sizeof(buf), "%s.X_ASUS_Index", path);
	sprintf(index, "%s", __get_parameter_value(buf, tmp));

	//tr_log(LOG_DEBUG, "get_ovpnc_field: index_str %s", index);

	if (strlen(index))
		value = atoi(index);

	return value;
}

int get_ovpnc_match_index(char *path) 
{
	char *nv, *nvp, *b;
	char *desc, *type, *index, *username, *password;
	//char index_t[16];
	node_t node;
	int found = 0, i = 1;
	int index_t = 0;
	char buf[256] = {0};

	snprintf(buf, sizeof(buf), "%s.X_ASUS_Description", path);

	if (lib_resolve_node(buf, &node) != 0)
		return found;

	index_t = get_ovpnc_index_value(node);

	//if (lib_resolve_node(path, &node_tmp) == 0 && lib_get_property(node_tmp, "name", index_t) == 0 && string_is_digits(index_t) == 1) {
	if (index_t) {
		nv = nvp = strdup(nvram_safe_get("vpnc_clientlist"));
		if (nv) {
			while ((b = strsep(&nvp, "<")) != NULL) {
				if ((vstrsep(b, ">", &desc, &type, &index, &username, &password) != 5))
					continue;
						
				//if (!strcmp(index_t, index)) {
				if (!strcmp(type, "OpenVPN") && index_t == atoi(index)) {
					found = i;
					break;
				}
				i++;
			}
			free(nv);
		}	
	}

	return found;
}

int del_ovpnc_autoreconn_field(char *path, int skip_index) 
{
	char *nv, *nvp, *b;
	//char index_t[16];
	//node_t node_tmp;
	char vpnc_autoreconn[64] = {0};
	int res = -1;

	memset(vpnc_autoreconn, 0, sizeof(vpnc_autoreconn));

	//if (lib_resolve_node(path, &node_tmp) == 0 && lib_get_property(node_tmp, "name", index_t) == 0 && string_is_digits(index_t) == 1) {

		nv = nvp = strdup(nvram_safe_get("vpnc_appendix"));
		if (nv) {
			int i = 0;

			while ((b = strsep(&nvp, "<")) != NULL) {
				char autoreconn[8];

				memset(autoreconn, 0, sizeof(autoreconn));

				i++;

				if (i == skip_index)
					continue;
				else
					sprintf(autoreconn, "%s", b);

				if (strlen(vpnc_autoreconn))
					strcat(vpnc_autoreconn, "<");

				if (strlen(autoreconn))
					strcat(vpnc_autoreconn, autoreconn);
			}
			free(nv);

			res = set_nvram("vpnc_appendix", vpnc_autoreconn);
		}
	//}

	return res;
}

int find_ovpnc_autoreconn_value(int index) 
{
	char *nv, *nvp, *b;
	int res = 0;

	nv = nvp = strdup(nvram_safe_get("vpnc_appendix"));
	if (nv) {
		int i = 1;
		while ((b = strsep(&nvp, "<")) != NULL) {	
			if (i == index) {
				res = atoi(b);
				break;
			}
			i++;
		}
		free(nv);
	}

	return res;
}

int set_vpnc_clientx_eas() 
{
	char *nv, *nvp, *b;
	char *desc, *type, *index, *username, *password;
	char buf[16] = {0};
	int res = -1;

	memset(buf, 0, sizeof(buf));

	nv = nvp = strdup(nvram_safe_get("vpnc_clientlist"));
	if (nv) {
		int i = 0;
		while ((b = strsep(&nvp, "<")) != NULL) {
			i++;
			if ((vstrsep(b, ">", &desc, &type, &index, &username, &password) != 5))
				continue;
		
			if (!strcmp(type, "OpenVPN")) {
				int result = find_ovpnc_autoreconn_value(i);
				char tmp[8];

				memset(tmp, 0, sizeof(tmp));
				
				if (result)
					sprintf(tmp, "%s,", index);

				if (strlen(tmp))
					strcat(buf, tmp);
			}					
		}
		free(nv);
		
		res = set_nvram("vpn_clientx_eas", buf);
	}	
	
	return res;
}

int del_ovpnc_list(int skip_index)
{
	char *nv, *nvp, *b;
	char *desc, *type, *index, *username, *password;
	char vpnc_clientlist[1024] = {0}, vpnc_rule[128] = {0};
	int res = -1, i = 0;

	memset(vpnc_clientlist, 0, sizeof(vpnc_clientlist));

	nv = nvp = strdup(nvram_safe_get("vpnc_clientlist"));
	if (nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			i++;

			if (i == skip_index)
				continue;

			memset(vpnc_rule, 0, sizeof(vpnc_rule));

			if ((vstrsep(b, ">", &desc, &type, &index, &username, &password) != 5))
				continue;
				
			snprintf(vpnc_rule, sizeof(vpnc_rule), "%s>%s>%s>%s>%s", desc, type, index, username, password);

			if (strlen(vpnc_clientlist))
				strcat(vpnc_clientlist, "<");

			if (strlen(vpnc_rule))
				strcat(vpnc_clientlist, vpnc_rule);
		}
		free(nv);

		res = set_nvram("vpnc_clientlist", vpnc_clientlist);
	}

	return res;
}

static int add_ovpnc(node_t node, char *arg, int nin)
{
	char buf[128] = {0}, nin_str[8] = {0};
	int res = 0;

	/* Add auto-reconnect to vpnc_appendix */
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "%s", nvram_safe_get("vpnc_appendix"));
	if (strlen(buf))
		strcat(buf, "<1");
	else
		sprintf(buf, "1");
	nvram_set("vpnc_appendix", buf);

	/* Add auto-reconnect to vpn_clientx_eas */
	memset(buf, 0, sizeof(buf));
	sprintf(nin_str, "%d,", nin);
	sprintf(buf, "%s", nvram_safe_get("vpn_clientx_eas"));
	if (!strstr(buf, nin_str)) {
		strcat(buf, nin_str);
		nvram_set("vpn_clientx_eas", buf);
	}

	return res;
}

static int del_ovpnc(node_t node, char *arg, int nin)
{
	char buf[256] = {0};
	int skip_index = 0;
	char *path = lib_node2path(node);
	int res = -1;
	tr_log(LOG_DEBUG, "del_ovpnc: res %d", res);
	path[strlen(path) - 1] = '\0';

	memset(buf, 0, sizeof(buf));

	snprintf(buf, sizeof(buf), "%s.%d", path, nin);
	skip_index = get_ovpnc_match_index(buf);
	
	res = del_ovpnc_autoreconn_field(buf, skip_index);

	if (!res) {
		res = set_vpnc_clientx_eas();
		tr_log(LOG_DEBUG, "del_ovpnc - set_vpnc_clientx_eas : res %d", res);
	}

	if (!res) {
		res = del_ovpnc_list(skip_index);
		tr_log(LOG_DEBUG, "del_ovpnc - del_ovpnc_list : res %d", res);
	}

	if (!res) {
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "%s.%d.X_ASUS_Description", path, nin);
		dev_notify("stop_vpnclient", buf, 0);
		reset_client_setting(nin);
		tr_log(LOG_DEBUG, "del_ovpnc - reset_client_setting : res %d", res);
	}

	if (!res) {
		reasign_instance_name(node, nin, 0);
		tr_log(LOG_DEBUG, "del_ovpnc - reasign_instance_name : res %d", res);
	}

	return res;
}

void get_ovpnc_field(node_t node, int field, char *buf) 
{
	char *nv, *nvp, *b;
	char *desc, *type, *index, *username, *password;
	//char *path = lib_node2path(node->parent);
	//char index_t[16];
	//node_t node_tmp;
#if 0
	path[strlen(path) - 1] = '\0';

	char pbuf[256];
	char index_str[8];
	int index_t = 0;
	char tmp_s[64];
	snprintf(pbuf, sizeof(pbuf), "%s.X_ASUS_Index", path);
	sprintf(index_str, "%s", __get_parameter_value(pbuf, tmp_s));

	tr_log(LOG_DEBUG, "get_ovpnc_field: index_str %s", index_str);

	if (strlen(index_str))
		index_t = atoi(index_str);
#endif
	int index_t = get_ovpnc_index_value(node);

	//if (lib_resolve_node(path, &node_tmp) == 0 && lib_get_property(node_tmp, "name", index_t) == 0 && string_is_digits(index_t) == 1) {
		nv = nvp = strdup(nvram_safe_get("vpnc_clientlist"));
		if (nv) {
			while ((b = strsep(&nvp, "<")) != NULL) {

				if ((vstrsep(b, ">", &desc, &type, &index, &username, &password) != 5))
					continue;
						
				//if (!strcmp(index_t, index)) {
				if (!strcmp(type, "OpenVPN") && strlen(index) && index_t == atoi(index)) {
					if (field == OVPNC_INDEX)
						sprintf(buf, "%s", index);
					else if (field == OVPNC_DESCRIPTION)
						sprintf(buf, "%s", desc);
					else if (field == OVPNC_USERNAME)
						sprintf(buf, "%s", username);
					else if (field == OVPNC_PASSWORD)
						sprintf(buf, "%s", password);

					break;
				}
			}
			free(nv);
		}	
	//}
}

int set_ovpnc_field(node_t node, int field, char *value)
{
	char *nv, *nvp, *b;
	char *desc, *type, *index, *username, *password;
	char vpnc_clientlist[1024] = {0}, vpnc_rule[128] = {0};
	int res = -1;

	memset(vpnc_clientlist, 0, sizeof(vpnc_clientlist));
	int index_t = get_ovpnc_index_value(node);

	//if (lib_resolve_node(path, &node_tmp) == 0 && lib_get_property(node_tmp, "name", index_t) == 0 && string_is_digits(index_t) == 1) {
		int found = 0;

		nv = nvp = strdup(nvram_safe_get("vpnc_clientlist"));
		if (nv) {
			while ((b = strsep(&nvp, "<")) != NULL) {
				memset(vpnc_rule, 0, sizeof(vpnc_rule));

				if ((vstrsep(b, ">", &desc, &type, &index, &username, &password) != 5))
					continue;
				
				//if (!strcmp(index_t, index))
				if (!strcmp(type, "OpenVPN") && strlen(index) && index_t == atoi(index))
					found = 1;

				if (field == OVPNC_DESCRIPTION)
					snprintf(vpnc_rule, sizeof(vpnc_rule), "%s>%s>%s>%s>%s", (!strcmp(type, "OpenVPN") && strlen(index) && index_t == atoi(index)) ? value : desc, type, index, username, password);
				else if (field == OVPNC_USERNAME)
					snprintf(vpnc_rule, sizeof(vpnc_rule), "%s>%s>%s>%s>%s", desc, type, index, (!strcmp(type, "OpenVPN") && strlen(index) && index_t == atoi(index)) ? value : username, password);
				else if (field == OVPNC_PASSWORD)
					snprintf(vpnc_rule, sizeof(vpnc_rule), "%s>%s>%s>%s>%s", desc, type, index, username, (!strcmp(type, "OpenVPN") && strlen(index) && index_t == atoi(index)) ? value : password);
				
				if (strlen(vpnc_clientlist))
					strcat(vpnc_clientlist, "<");

				if (strlen(vpnc_rule))
					strcat(vpnc_clientlist, vpnc_rule);
			}
			free(nv);
		}

		if (!found) {
			memset(vpnc_rule, 0, sizeof(vpnc_rule));

			if (field == OVPNC_INDEX)
				snprintf(vpnc_rule, sizeof(vpnc_rule), ">OpenVPN>%s>>", value);				
			else if (field == OVPNC_DESCRIPTION)
				snprintf(vpnc_rule, sizeof(vpnc_rule), "%s>OpenVPN>0>>", value);
			else if (field == OVPNC_USERNAME)
				snprintf(vpnc_rule, sizeof(vpnc_rule), ">OpenVPN>0>%s>", value);
			else if (field == OVPNC_PASSWORD)
				snprintf(vpnc_rule, sizeof(vpnc_rule), ">OpenVPN>0>>%s", value);

			if (strlen(vpnc_clientlist))
				strcat(vpnc_clientlist, "<");

			if (strlen(vpnc_rule))
				strcat(vpnc_clientlist, vpnc_rule);
		}
	//}

	tr_log(LOG_DEBUG, "set_ovpnc_field: vpnc_clientlist %s", vpnc_clientlist);
	res = set_nvram("vpnc_clientlist", vpnc_clientlist);

	return res;	
}

static int set_ovpnc_index(node_t node, char *arg, char *value)
{
	int res = set_ovpnc_field(node, OVPNC_INDEX, value);

#ifdef RTCONFIG_SFEXPRESS
	char buf[64] = {0};

	if (!res) {
		snprintf(buf, sizeof(buf), "vpn_client%s_sf", value);
		res = set_nvram(buf, "1");
	}
#endif

	return res;
}

static int get_ovpnc_index(node_t node, char *arg, char **value)
{
	char buf[64] = {0};

	memset(buf, 0, sizeof(buf));
	get_ovpnc_field(node, OVPNC_INDEX, buf);
	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_ovpnc_desc(node_t node, char *arg, char *value)
{
	int res = 0;
	
	res = set_ovpnc_field(node, OVPNC_DESCRIPTION, value);

	return res;
}

static int get_ovpnc_desc(node_t node, char *arg, char **value)
{
	char buf[64] = {0};
	
	memset(buf, 0, sizeof(buf));
	get_ovpnc_field(node, OVPNC_DESCRIPTION, buf);
	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int get_ovpnc_username(node_t node, char *arg, char **value)
{
	char buf[64] = {0};
	
	memset(buf, 0, sizeof(buf));
	get_ovpnc_field(node, OVPNC_USERNAME, buf);
	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_ovpnc_username(node_t node, char *arg, char *value)
{
	int res = 0;
	tr_log(LOG_DEBUG, "set_ovpnc_username");
	//res = set_ovpnc_field(node, OVPNC_USERNAME, value, get_ovpnc_index_field(node));
	res = set_ovpnc_field(node, OVPNC_USERNAME, value);

	if (!res) {
		char *path = lib_node2path(node->parent);
		//char index_t[8];
		//node_t node_tmp;
		int index_t = get_ovpnc_index_value(node);

		path[strlen(path) - 1] = '\0';

		//if (lib_resolve_node(path, &node_tmp) == 0 && lib_get_property(node_tmp, "name", index_t) == 0 && 
		//	string_is_digits(index_t) == 1) {
		if (index_t) {
			char nv[32];
			sprintf(nv, "vpn_client%d_username", index_t);
			res = set_nvram(nv, value);
		}
		else
			res = -1;
	}

	return res;
}

static int get_ovpnc_password(node_t node, char *arg, char **value)
{
	char buf[64] = {0};
	
	memset(buf, 0, sizeof(buf));
	get_ovpnc_field(node, OVPNC_PASSWORD, buf);
	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_ovpnc_password(node_t node, char *arg, char *value)
{
	int res = 0;
	
	//res = set_ovpnc_field(node, OVPNC_PASSWORD, value, get_ovpnc_index_field(node));
	res = set_ovpnc_field(node, OVPNC_PASSWORD, value);

	if (!res) {
		char *path = lib_node2path(node->parent);
		//char index_t[8];
		//node_t node_tmp;
		int index_t = get_ovpnc_index_value(node);

		path[strlen(path) - 1] = '\0';

		//if (lib_resolve_node(path, &node_tmp) == 0 && lib_get_property(node_tmp, "name", index_t) == 0 && 
		//	string_is_digits(index_t) == 1) {
		if (index_t) {
			char nv[32];
			sprintf(nv, "vpn_client%d_password", index_t);
			res = set_nvram(nv, value);
		}
		else
			res = -1;
	}

	return res;
}

void get_ovpnc_autoreconn_field(node_t node, int match_index, char *buf) 
{
	char *nv, *nvp, *b;
	//char *path = lib_node2path(node->parent);
	//char index_t[16];
	//node_t node_tmp;
	
	//path[strlen(path) - 1] = '\0';

	//if (lib_resolve_node(path, &node_tmp) == 0 && lib_get_property(node_tmp, "name", index_t) == 0 && string_is_digits(index_t) == 1) {
	//if (index_t)
		nv = nvp = strdup(nvram_safe_get("vpnc_appendix"));
		if (nv) {
			int i = 1;

			while ((b = strsep(&nvp, "<")) != NULL) {
				if (i == match_index) {
					if (!strcmp(b, "1"))
						sprintf(buf, "true");
					else
						sprintf(buf, "false");
					break;
				}
				i++;
			}
			free(nv);
		}	
	//}
}

int set_ovpnc_autoreconn_field(node_t node, int match_index, char *value) 
{
	char *nv, *nvp, *b;
	//char *path = lib_node2path(node->parent);
	//char index_t[16];
	//node_t node_tmp;
	char vpnc_autoreconn[64];
	int res = -1;

	//path[strlen(path) - 1] = '\0';

	memset(vpnc_autoreconn, 0, sizeof(vpnc_autoreconn));

	//if (lib_resolve_node(path, &node_tmp) == 0 && lib_get_property(node_tmp, "name", index_t) == 0 && string_is_digits(index_t) == 1) {
		nv = nvp = strdup(nvram_safe_get("vpnc_appendix"));
		if (nv) {
			int i = 1;

			while ((b = strsep(&nvp, "<")) != NULL) {
				char autoreconn[8];

				memset(autoreconn, 0, sizeof(autoreconn));

				if (i == match_index)
					sprintf(autoreconn, "%d", string2boolean(value));
				else
					sprintf(autoreconn, "%s", b);

				i++;

				if (strlen(vpnc_autoreconn))
					strcat(vpnc_autoreconn, "<");

				if (strlen(autoreconn))
					strcat(vpnc_autoreconn, autoreconn);
			}
			free(nv);
			
			res = set_nvram("vpnc_appendix", vpnc_autoreconn);
		}
	//}

	return res;
}

static int get_ovpnc_autoreconn(node_t node, char *arg, char **value)
{
	char buf[64] = {0};
	int match_index = 0;
	char *path = lib_node2path(node->parent);

	path[strlen(path) - 1] = '\0';

	memset(buf, 0, sizeof(buf));

	match_index = get_ovpnc_match_index(path);
	get_ovpnc_autoreconn_field(node, match_index, buf);
	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int set_ovpnc_autoreconn(node_t node, char *arg, char *value)
{
	int res = 0, match_index = 0;
	char *path = lib_node2path(node->parent);

	path[strlen(path) - 1] = '\0';

	match_index = get_ovpnc_match_index(path);
	res = set_ovpnc_autoreconn_field(node, match_index, value);
	if (!res)
		res = set_vpnc_clientx_eas();

	return res;
}

#define VPN_CLIENT_UPLOAD	"/tmp/openvpn_file"
enum
{
	TYPE_OVPN,
	TYPE_CA,
	TYPE_CLIENT_CERT,
	TYPE_CLIENT_KEY,
	TYPE_STATIC_KEY
};

int save_cert_key(node_t node, int file_type, char *value)
{
	int res = -1;
	FILE *fifo = NULL;
	char nv[32] = {0}, path[256] = {0}, tmp[64] = {0}
	char *parent_path = lib_node2path(node->parent);
	int index_t = 0;

	parent_path[strlen(parent_path) - 1] = '\0';
	memset(path, 0, sizeof(path));
	snprintf(path, sizeof(path), "%s", parent_path);
	index_t = get_ovpnc_index_value(node);

	//if (lib_resolve_node(path, &node_tmp) == 0 && lib_get_property(node_tmp, "name", index_t) == 0 && string_is_digits(index_t) == 1) {
	if (index_t) {
		if (!(fifo = fopen(VPN_CLIENT_UPLOAD, "w")))
			return -1;

		fputs(value, fifo);
		fclose(fifo);

		if (file_type == TYPE_OVPN) {
			char buf[256] = {0}, username_str[64] = {0}, password_str[64] = {0};
			
			memset(buf, 0, sizeof(buf));

			reset_client_setting(index_t);
			res = read_ovpn_config_file(VPN_CLIENT_UPLOAD, index_t);

			/* Reset username for openvpn client */
			snprintf(buf, sizeof(buf), "%s.X_ASUS_Username", path);
			//tr_log(LOG_DEBUG, "save_cert_key - path: %s", buf);
			sprintf(username_str, "%s", __get_parameter_value(buf, tmp));
			//tr_log(LOG_DEBUG, "save_cert_key - username: %s", username_str);
			sprintf(nv, "vpn_client%d_username", index_t);
			set_nvram(nv, username_str);
			
			/* Reset password for openvpn client */
			snprintf(buf, sizeof(buf), "%s.X_ASUS_Password", path);
			//tr_log(LOG_DEBUG, "save_cert_key - path: %s", buf);
			sprintf(password_str, "%s", __get_parameter_value(buf, tmp));
			//tr_log(LOG_DEBUG, "save_cert_key - password: %s", password_str);
			sprintf(nv, "vpn_client%d_password", index_t);
			set_nvram(nv, password_str);
		}
		else if (file_type == TYPE_CA)
			sprintf(nv, "vpn_crt_client%d_ca", index_t);
		else if (file_type == TYPE_CLIENT_CERT)
			sprintf(nv, "vpn_crt_client%d_crt", index_t);
		else if (file_type == TYPE_CLIENT_KEY)
			sprintf(nv, "vpn_crt_client%d_key", index_t);
		else if (file_type == TYPE_STATIC_KEY)
			sprintf(nv, "vpn_crt_client%d_static", index_t);
		else
			return -1;

		if (file_type != TYPE_OVPN)	
			res = set_crt_parsed(nv, VPN_CLIENT_UPLOAD);
	}

	return res;	
}

int openvpn_client_file(const char *type, char *path)
{
	int index = 0, res = -1;
	char tmp[64] = {0};

	if (!strcmp(type, "4 Openvpn Client1 File"))
		index = 1;
	else if (!strcmp(type, "5 Openvpn Client2 File"))
		index = 2;		
	else if (!strcmp(type, "6 Openvpn Client3 File"))
		index = 3;

	if (index) {
		char buf[256] = {0}, username_str[64] = {0}, password_str[64] = {0}, nv[32] = {0};
			
		memset(buf, 0, sizeof(buf));

		reset_client_setting(index);
		res = read_ovpn_config_file(VPN_CLIENT_UPLOAD, index);

		/* Reset username for openvpn client */
		snprintf(buf, sizeof(buf), "%s.X_ASUS_Username", path);
		sprintf(username_str, "%s", __get_parameter_value(buf, tmp));
		sprintf(nv, "vpn_client%d_username", index);
		set_nvram(nv, username_str);
			
		/* Reset password for openvpn client */
		snprintf(buf, sizeof(buf), "%s.X_ASUS_Password", path);
		sprintf(password_str, "%s", __get_parameter_value(buf, tmp));
		sprintf(nv, "vpn_client%d_password", index);
		set_nvram(nv, password_str);
	}

	return res;
}

static int set_ovpnc_ovpnfile(node_t node, char *arg, char *value)
{
	return save_cert_key(node, TYPE_OVPN, value);
}

static int set_ovpnc_ca(node_t node, char *arg, char *value)
{
	return save_cert_key(node, TYPE_CA, value);
}

static int set_ovpnc_cert(node_t node, char *arg, char *value)
{
	return save_cert_key(node, TYPE_CLIENT_CERT, value);
}

static int set_ovpnc_clientkey(node_t node, char *arg, char *value)
{
	return save_cert_key(node, TYPE_CLIENT_KEY, value);
}

static int set_ovpnc_statickey(node_t node, char *arg, char *value)
{
	return save_cert_key(node, TYPE_STATIC_KEY, value);
}

static int get_ovpnc_status(node_t node, char *arg, char **value)
{
	char *path = lib_node2path(node->parent);
	char buf[32] = {0};
	int index_t = get_ovpnc_index_value(node);

	memset(buf, 0, sizeof(buf));

	path[strlen(path) - 1] = '\0';

	//if (lib_resolve_node(path, &node_tmp) == 0 && lib_get_property(node_tmp, "name", index_t) == 0 && string_is_digits(index_t) == 1) {
	if (index_t) {
		char nv[32];
		int state = 0;

		sprintf(nv, "vpn_client%d_state", index_t);
		state = nvram_get_int(nv);

		if (state == 0)
			sprintf(buf, "Disconnected");			
		else if (state == 1)
			sprintf(buf, "Connecting");
		else if (state == 2)
			sprintf(buf, "Connected");
		else
			sprintf(buf, "Error");

		*value = strdup(buf);
		return *value ? 0 : -1;	
	}
	return -1;
}
#endif	//RTCONFIG_SFEXPRESS

/* handler table */
static struct handler handlers[] = {
	/* for DeviceInfo */
	{ "nvram",	{{ get_nvram_value, set_nvram_value }}},
	{ "serial",	{{ get_serial, NULL }}},
	{ "oui",	{{ get_oui, NULL }}},
	{ "firmver",	{{ get_firmver, NULL }}},
	{ "uptime",	{{ get_uptime, NULL }}},
	{ "conn_url",	{{ get_conn_url, NULL }}},

	/* for time */
	{ "currentlocaltime",	{{ get_currentlocaltime, NULL }}},
#ifdef ASUSWRT
	{ "localtimezonename",	{{ get_localtimezonename, set_localtimezonename }}},
#endif
#ifdef DSL_ASUSWRT
	{ "localtimezone",	{{ get_localtimezone, set_localtimezone }}},
	{ "daylightsavingsused",	{{ get_daylightsavingsused, set_daylightsavingsused }}},
#endif

	/* for userinterface */
	{ "availablelanguages", 	{{get_availablelanguages, NULL}}},
#ifdef DSL_ASUSWRT
	{ "currentlang", 	{{get_currentlang, set_currentlang}}},
#endif

	/* deny_object */
	{ "deny_object",	{{ deny_add_object, deny_del_object }}},

	/* for device info */
	{ "totalmemory", 		{{get_total_memory, NULL }}},
	{ "freememory", 		{{get_free_memory, NULL }}},

#ifdef TR098
	/* for forwarding */
	{ "l3_forwarding",	{{ add_l3_forwarding, del_l3_forwarding }}},
	{ "l3_destip",		{{ NULL, set_l3_config }}},
	{ "l3_destnetmask",	{{ NULL, set_l3_config }}},
	{ "l3_gatewayip",	{{ NULL, set_l3_config }}},
	{ "l3_iface",		{{ NULL, set_l3_config }}},
	{ "l3_metric",		{{ NULL, set_l3_config }}},

	/* for config of lan host */
#ifdef DSL_ASUSWRT
	{ "lanhost_dhcpserverenable",	{{ get_lanhost_dhcpserverenable, set_lanhost_dhcpserverenable }}},
#endif
	{ "lanhost_dnsservers",	{{ get_lanhost_dnsservers, set_lanhost_dnsservers }}},

	/* for dhcp static address of lan host */
	{ "lanhost_dhcpstatic",	{{ NULL, del_lanhost_dhcpstatic }}},
	{ "lanhost_mac",	{{ NULL, set_lanhost_dhcpstatic }}},
	{ "lanhost_ip",		{{ NULL, set_lanhost_dhcpstatic }}},

#ifdef RTCONFIG_XDSL
	/* for WANDSLInterfaceConfig of dsl */
	{ "dsl_if_config",	{{ get_dsl_if_config, NULL }}},

	/* for wanconnectiondevice of dsl */
	{ "dsl_wan_connection_device",	{{ add_dsl_wan_connection_device, del_dsl_wan_connection_device }}},

	/* for WANDSLLinkConfig of dsl */
	{ "dsl_enable",			{{ get_dsl_enable, set_dsl_enable }}},
	{ "dsl_link_type",		{{ get_dsl_link_type, set_dsl_link_type }}},
	{ "dsl_dest_addr",		{{ get_dsl_dest_addr, set_dsl_dest_addr }}},
	{ "dsl_atm_encap",		{{ get_dsl_atm_encap, set_dsl_atm_encap }}},
	{ "dsl_atm_qos",		{{ get_dsl_atm_qos, set_dsl_atm_qos }}},
	{ "dsl_atm_pcr",		{{ get_dsl_atm_pcr, set_dsl_atm_pcr }}},
	{ "dsl_atm_mbs",		{{ get_dsl_atm_mbs, set_dsl_atm_mbs }}},	
	{ "dsl_atm_scr",		{{ get_dsl_atm_scr, set_dsl_atm_scr }}},
	{ "dsl_enable_dot1q",	{{ get_dsl_enable_dot1q, set_dsl_enable_dot1q }}},	
	{ "dsl_dot1q_vid",		{{ get_dsl_dot1q_vid, set_dsl_dot1q_vid }}},

	/* for wanip of dsl */
	{ "dsl_wanip_connection",	{{ add_dsl_wanipconnection, del_dsl_wanipconnection }}},

	/* for wanppp of dsl */
	{ "dsl_wanppp_connection",	{{ add_dsl_wanpppconnection, del_dsl_wanpppconnection }}},
#endif

    /* for get device config file*/ //2016.6.28 sherry add
     { "dev_conffile",		{{ get_dev_conffile, set_dev_conffile}}},

	/* for ethernet link status */ //2016.6.28 sherry add
	{ "eth_linkstatus",		{{ get_eth_phylinkstatus, NULL }}},
	 /*for system log*/
    	{ "dev_syslog",		{{ get_dev_syslog, NULL}}},
	/* for wanip of ethernet */
	{ "eth_wanip_connection",	{{ add_eth_wanipconnection, del_eth_wanipconnection }}},
	{ "eth_wanip_enable",		{{ get_eth_enable, set_eth_enable }}},
	{ "eth_wanip_connectionstatus",	{{ get_eth_connectionstatus, NULL }}},
	{ "eth_wanip_natenabled",	{{ get_eth_natenabled, set_eth_natenabled }}},
	{ "eth_wanip_addressingtype",	{{ get_eth_addressingtype, set_eth_addressingtype }}},
	{ "eth_wanip_externalipaddress",	{{ get_eth_externalipaddress, set_eth_externalipaddress }}},
	{ "eth_wanip_subnetmask",	{{ get_eth_subnetmask, set_eth_subnetmask }}},
	{ "eth_wanip_defaultgateway",	{{ get_eth_defaultgateway, set_eth_defaultgateway }}},
	{ "eth_wanip_dnsoverrideallowed",	{{ get_eth_dnsoverrideallowed, set_eth_dnsoverrideallowed }}},
	{ "eth_wanip_dnsservers",	{{ get_eth_dnsservers, set_eth_dnsservers }}},

	/* for wanppp of ethernet */
	{ "eth_wanppp_connection",	{{ add_eth_wanpppconnection, del_eth_wanpppconnection }}},
	{ "eth_wanppp_enable",	{{ get_eth_enable, set_eth_enable }}},
	{ "eth_wanppp_connectionstatus",	{{ get_eth_connectionstatus, NULL }}},
	{ "eth_wanppp_natenabled",	{{ get_eth_natenabled, set_eth_natenabled }}},
	{ "eth_wanppp_externalipaddress",	{{ get_eth_externalipaddress, NULL }}},	
	{ "eth_wanppp_defaultgateway",	{{ get_eth_defaultgateway, NULL }}},
	{ "eth_wanppp_idledisconnecttime",	{{ get_eth_idledisconnecttime, set_eth_idledisconnecttime }}},
	{ "eth_wanppp_username",	{{ get_eth_username, set_eth_username }}},
	{ "eth_wanppp_password",	{{ get_eth_password, set_eth_password }}},
	{ "eth_wanppp_transporttype",	{{ get_eth_transporttype, NULL }}},
	{ "eth_wanppp_pppoeacname",	{{ get_eth_pppoeacname, set_eth_pppoeacname }}},
	{ "eth_wanppp_pppoeservicename",	{{ get_eth_pppoeservicename, set_eth_pppoeservicename }}},
	{ "eth_wanppp_dnsservers",	{{ get_eth_dnsservers, set_eth_dnsservers }}},

	/* for wan ip and ppp stats */
	{ "eth_wanip_bytessent",	{{ get_eth_bytessent, NULL }}},
	{ "eth_wanip_bytesreceived",	{{ get_eth_bytesreceived, NULL }}},
	{ "eth_wanip_packetssent",	{{ get_eth_packetssent, NULL }}},
	{ "eth_wanip_packetsreceived",	{{ get_eth_packetsreceived, NULL }}},
	{ "eth_wanip_errorssent",	{{ get_eth_errorssent, NULL }}},
	{ "eth_wanip_errorsreceived",	{{ get_eth_errorsreceived, NULL }}},
	{ "eth_wanip_discardpacketssent",	{{ get_eth_discardpacketssent, NULL }}},
	{ "eth_wanip_discardpacketsreceived",	{{ get_eth_discardpacketsreceived, NULL }}},

	{ "eth_wanppp_bytessent",	{{ get_eth_bytessent, NULL }}},
	{ "eth_wanppp_bytesreceived",	{{ get_eth_bytesreceived, NULL }}},
	{ "eth_wanppp_packetssent",	{{ get_eth_packetssent, NULL }}},
	{ "eth_wanppp_packetsreceived",	{{ get_eth_packetsreceived, NULL }}},
	{ "eth_wanppp_errorssent",	{{ get_eth_errorssent, NULL }}},
	{ "eth_wanppp_errorsreceived",	{{ get_eth_errorsreceived, NULL }}},
	{ "eth_wanppp_discardpacketsSent",	{{ get_eth_discardpacketssent, NULL }}},
	{ "eth_wanppp_discardpacketsreceived",	{{ get_eth_discardpacketsreceived, NULL }}},

	/* for port mapping */
	{ "eth_portmapping",	{{ add_eth_portmapping, del_eth_portmapping }}},
	{ "eth_remotehost",	{{ NULL, set_port_mapping }}},
	{ "eth_externalport",	{{ NULL, set_port_mapping }}},
	{ "eth_externalportendrange",	{{ NULL, set_port_mapping }}},
	{ "eth_internalport",	{{ NULL, set_port_mapping }}},
	{ "eth_portmappingprotocol",	{{ NULL, set_port_mapping }}},
	{ "eth_internalclient",	{{ NULL, set_port_mapping }}},
	{ "eth_portmappingdescription",	{{ NULL, set_port_mapping }}},

	/*for wlan of landevice*/
	{ "lan_wlan_bssid",  	{{get_wlan_bssid, NULL }}},
	{ "lan_wlan_channel", 	{{get_wlan_channel, set_wlan_channel}}},
	{ "lan_wlan_name", 	{{get_wlan_name,  NULL  }}},
	{ "lan_wlan_autochannel_enable",{{get_wlan_autochannel_enable, set_wlan_autochannel_enable }}},
	{ "lan_wlan_ssid",  		{{get_wlan_ssid, set_wlan_ssid }}},
	{ "lan_wlan_beacontype",  		{{get_wlan_beacontype, set_wlan_beacontype }}},
	{ "lan_wlan_ssid_endable",  	{{get_wlan_ssid_enable, set_wlan_ssid_enable }}},
	{ "lan_wlan_macaddrcontrolenable",  	{{get_wlan_macaddrcontrolenable, set_wlan_macaddrcontrolenable }}},
	{ "lan_wlan_wepkeyindex",  		{{get_wlan_wepkeyindex,  set_wlan_wepkeyindex }}},
	{ "lan_wlan_keypassphrase",  		{{get_wlan_keypassphrase,  set_wlan_keypassphrase  }}},
	{ "lan_wlan_wepencryptionlevel",  	{{get_wlan_wepencryptionlevel, NULL  }}},
	{ "lan_wlan_basicencrytionmodes",  	{{get_wlan_basicencrytionmodes, set_wlan_basicencrytionmodes }}},
	{ "lan_wlan_basicauthenticationmodes",  	{{get_wlan_basicauthenticationmodes, set_wlan_basicauthenticationmodes }}},
	{ "lan_wlan_wpaencryptionmodes",  	{{get_wlan_wpaencrytionmodes, set_wlan_wpaencrytionmodes  }}},
	{ "lan_wlan_wpaauthenticationmode", 	{{get_wlan_wpaauthenticationmode, set_wlan_wpaauthenticationmode}}},
	{ "lan_wlan_ieee11iencryptionmodes", 	{{get_wlan_ieee11iencryptionmodes, set_wlan_ieee11iencryptionmodes }}},
	{ "lan_wlan_ieee11iauthenticationmode", {{get_wlan_ieee11iauthenticationmode, set_wlan_ieee11iauthenticationmode }}},
	{ "lan_wlan_radioenable",  		{{get_wlan_radioenable, set_wlan_radioenable  }}},
	{ "lan_wlan_totalassociations",		{{get_wlan_totalassociations, NULL }}},
	{ "lan_wlan_wpsenable",  		{{get_wlan_wpsenable,  set_wlan_wpsenable }}},
	{ "lan_wlan_wpsdevicepassword",  	{{get_wlan_wpsdevicepassword, set_wlan_wpsdevicepassword }}},
	{ "lan_wlan_wpsconfigmethodsenabled", 	{{get_wlan_wpsconfigmethodsenabled, set_wlan_wpsconfigmethodsenabled }}},	
	{ "lan_wlan_wpsconfigurationstate",  	{{get_wlan_wpsconfigurationstate, NULL }}},
	{ "lan_wlan_wep_key",    		{{get_wlan_wep_key, set_wlan_wep_key }}},
	{ "lan_wlan_presharedkey", 		{{get_wlan_presharedkey, set_wlan_presharedkey }}},
	{ "allowedmacaddress", 			{{get_wlan_allowedmacaddress, set_wlan_allowedmacaddress }}},

	/* for wlan stats of landevice */
	{ "lan_wlan_totalbytessent",		{{ get_wlan_totalbytessent, NULL }}},
	{ "lan_wlan_totalbytesreceived",	{{ get_wlan_totalbytesreceived, NULL }}},
	{ "lan_wlan_totalpacketssent",		{{ get_wlan_totalpacketssent, NULL }}},
	{ "lan_wlan_totalpacketsreceived",	{{ get_wlan_totalpacketsreceived, NULL }}},
	{ "lan_wlan_errorssent",		{{ get_wlan_errorssent, NULL }}},
	{ "lan_wlan_errorsreceived",		{{ get_wlan_errorsreceived, NULL }}},
	{ "lan_wlan_discardpacketssent",	{{ get_wlan_discardpacketssent, NULL }}},
	{ "lan_wlan_discardpacketsreceived",	{{ get_wlan_discardpacketsreceived, NULL }}},
#endif

#ifdef TR181
	/* for userinterface.Remoteaccess */
	{ "userinterface_port",		{{get_userinterface_port, set_userinterface_port }}},
	{ "userinterface_support_protocols",	{{get_support_protocols, NULL }}},
	{ "userinterface_protocol",	{{get_userinterface_protocol, set_userinterface_protocol }}},

	{ "user_name",			{{get_user_name, set_user_name }}},

	/* for radio of wifi */
	{ "wifi_radio_enable", 			{{get_wifi_radio_enable, set_wifi_radio_enable }}},
	{ "wifi_radio_name",			{{get_wifi_radio_name, NULL}}},
	{ "wifi_radio_frequencybands",		{{get_wifi_radio_frequencybands, NULL}}},
	{ "wifi_radio_standards",		{{get_wifi_radio_standards, NULL}}},
	/* for Cellular if */
    	{"usim_if_enable",        {{ get_usim_enable, NULL }}},
#ifdef ASUSWRT
	{ "wifi_radio_possiblechannels", 	{{get_wifi_radio_possiblechannels, NULL }}},
#endif	
	{ "wifi_radio_channel", 		{{get_wifi_radio_channel, set_wifi_radio_channel }}},
	{ "wifi_radio_autochannelenable", 	{{get_wifi_radio_autochannelenable, set_wifi_radio_autochannelenable }}},
	{ "wifi_radio_operatingchannelbandwidth", {{get_wifi_radio_operatingchannelbandwidth, set_wifi_radio_operatingchannelbandwidth }}},
	{ "wifi_radio_extensionchannel", 	{{get_wifi_radio_extensionchannel, set_wifi_radio_extensionchannel }}},

	/* for radio's stats of wifi */
	{ "wifi_radio_totalbytessent",		{{ get_radio_totalbytessent, NULL }}},
	{ "wifi_radio_totalbytesreceived",		{{ get_radio_totalbytesreceived, NULL }}},
	{ "wifi_radio_totalpacketssent",		{{ get_radio_totalpacketssent, NULL }}},
	{ "wifi_radio_totalpacketsreceived",		{{ get_radio_totalpacketsreceived, NULL }}},
	{ "wifi_radio_errorssent",			{{ get_radio_errorssent, NULL }}},
	{ "wifi_radio_errorsreceived",		{{ get_radio_errorsreceived, NULL }}},
	{ "wifi_radio_discardpacketssent",		{{ get_radio_discardpacketssent, NULL }}},
	{ "wifi_radio_discardpacketsreceived",	{{ get_radio_discardpacketsreceived, NULL }}},

	/* for ssid of wifi */
	{ "wifi_ssid_enable", 			{{get_wifi_radio_enable, set_wifi_radio_enable }}},
	{ "wifi_ssid_bssid", 			{{get_wifi_radio_bssid, NULL }}},
	{ "wifi_ssid_ssid", 			{{get_wifi_radio_ssid, set_wifi_radio_ssid }}},

	/* for ssid's stats of wifi */
	{ "wifi_ssid_totalbytessent",		{{ get_wlan_totalbytessent, NULL }}},
	{ "wifi_ssid_totalbytesreceived",	{{ get_wlan_totalbytesreceived, NULL }}},
	{ "wifi_ssid_totalpacketssent",		{{ get_wlan_totalpacketssent, NULL }}},
	{ "wifi_ssid_totalpacketsreceived",	{{ get_wlan_totalpacketsreceived, NULL }}},
	{ "wifi_ssid_errorssent",		{{ get_wlan_errorssent, NULL }}},
	{ "wifi_ssid_errorsreceived",		{{ get_wlan_errorsreceived, NULL }}},
	{ "wifi_ssid_discardpacketssent",	{{ get_wlan_discardpacketssent, NULL }}},
	{ "wifi_ssid_discardpacketsreceived",	{{ get_wlan_discardpacketsreceived, NULL }}},

	/* ap of wifi */
	{ "wifi_ap_ssidadvertisementenabled", {{get_wifi_ap_ssidadvertisementenabled, set_wifi_ap_ssidadvertisementenabled}}},
	{ "wifi_ap_associatednum",		{{get_wifi_ap_associatednum, NULL}}},
	{ "wifi_ap_modeabled", 	{{get_wifi_ap_modeenabled, set_wifi_ap_modeenabled }}},
	{ "wifi_ap_wepkey", 		{{get_wifi_ap_wepkey, set_wifi_ap_wepkey }}},
	{ "wifi_ap_presharedkey", 	{{get_wifi_ap_presharedkey, set_wifi_ap_presharedkey }}},
	{ "wifi_ap_rekeyinginterval", 	{{get_wifi_ap_rekeyinginterval, set_wifi_ap_rekeyinginterval }}},
	{ "wifi_ap_radiusserip", 		{{get_wifi_ap_radiusserip, set_wifi_ap_radiusserip}}},
	{ "wifi_ap_radiusserport", 		{{get_wifi_ap_radiusserport, set_wifi_ap_radiusserport}}},
	{ "wifi_ap_radiussersecret", 		{{get_wifi_ap_radiussersecret, set_wifi_ap_radiussersecret}}},
	{ "wifi_ap_wps_enable", 		{{get_wifi_ap_wps_enable, set_wifi_ap_wps_enable}}},
	{ "wifi_ap_wps_configmethodsenabled", 	{{get_wifi_ap_wps_configmethodsenabled, set_wifi_ap_wps_configmethodsenabled}}},

#ifdef RTCONFIG_XDSL
	/* for line of dsl */
	{ "dsl_line_config",	{{ get_dsl_if_config, NULL }}},

	/* for link of atm */
	{ "atm_link",			{{ add_atm_link, del_atm_link }}},	
	{ "atm_enable",			{{ get_dsl_enable, set_dsl_enable }}},
	{ "atm_linktype",		{{ get_dsl_link_type, set_dsl_link_type }}},
	{ "atm_destaddr",		{{ get_dsl_dest_addr, set_dsl_dest_addr }}},
	{ "atm_encap",			{{ get_dsl_atm_encap, set_dsl_atm_encap }}},

	/* for qos of link (atm) */
	{ "atm_qos",			{{ get_dsl_atm_qos, set_dsl_atm_qos }}},	
	{ "atm_pcr",			{{ get_dsl_atm_pcr, set_dsl_atm_pcr }}},
	{ "atm_mbs",			{{ get_dsl_atm_mbs, set_dsl_atm_mbs }}},
	{ "atm_scr",			{{ get_dsl_atm_scr, set_dsl_atm_scr }}},

	/* for link of ptm */
	{ "ptm_link",			{{ add_ptm_link, del_ptm_link }}},
	{ "ptm_enable",			{{ get_dsl_enable, set_dsl_enable }}},
	{ "ptm_enable_dot1q",	{{ get_dsl_enable_dot1q, set_dsl_enable_dot1q }}},	
	{ "ptm_dot1q_vid",		{{ get_dsl_dot1q_vid, set_dsl_dot1q_vid }}},
#endif

	/* for ip if */
	{ "ip_if",	{{ add_ip_if, del_ip_if }}},
	{ "ip_if_enable",			{{ get_eth_enable, set_eth_enable }}},
	{ "ip_if_name",			{{ get_eth_name, NULL }}},
	{ "ip_if_lowerlayers",			{{ NULL, set_ip_if_lowerlayers }}},
	{ "ip_if_ipaddress",			{{get_ip_ipaddress, set_ip_ipaddress}}},
	{ "ip_if_subnetmask",			{{get_ip_subnetmask, set_ip_subnetmask}}},
	{ "ip_if_addressingtype",		{{ get_eth_addressingtype, NULL }}},

#ifdef RTCONFIG_IPV6
	{ "ip_if_ipv6_enable",			{{ get_ipv6_enable, set_ipv6_enable}}},
	{ "ip_if_ipv6_ipaddress",		{{ get_ipv6_address, set_ipv6_address}}},
#endif

	/* for ip if stats */
	{ "ip_if_bytessent",	{{ get_eth_bytessent, NULL }}},
	{ "ip_if_bytesreceived",	{{ get_eth_bytesreceived, NULL }}},
	{ "ip_if_packetssent",	{{ get_eth_packetssent, NULL }}},
	{ "ip_if_packetsreceived",	{{ get_eth_packetsreceived, NULL }}},
	{ "ip_if_errorssent",	{{ get_eth_errorssent, NULL }}},
	{ "ip_if_errorsreceived",	{{ get_eth_errorsreceived, NULL }}},
	{ "ip_if_discardpacketsSent",	{{ get_eth_discardpacketssent, NULL }}},
	{ "ip_if_discardpacketsreceived",	{{ get_eth_discardpacketsreceived, NULL }}},

	/* for ppp if */
	{ "ppp_if",	{{ add_ppp_if, del_ppp_if }}},
	{ "ppp_if_enable",			{{ get_eth_enable, set_eth_enable }}},
	{ "ppp_if_connectionstatus",	{{ get_eth_connectionstatus, NULL }}},
	{ "ppp_if_name",			{{ get_eth_name, NULL }}},
	{ "ppp_if_lowerlayers",			{{ NULL, set_ppp_if_lowerlayers }}},
	{ "ppp_if_idledisconnecttime", 	{{ get_eth_idledisconnecttime, set_eth_idledisconnecttime }}},
	{ "ppp_if_username", 		{{ get_eth_username, set_eth_username }}},
	{ "ppp_if_password", 		{{ get_eth_password, set_eth_password }}},
#ifdef ASUSWRT
	{ "ppp_if_mru", 	 		{{ get_MRU, NULL }}},
	{ "ppp_if_pppoe_acname",	{{ get_eth_pppoeacname, set_eth_pppoeacname }}},
	{ "ppp_if_pppoe_servicename",	{{ get_eth_pppoeservicename, set_eth_pppoeservicename }}},
#endif
	{ "ppp_if_ipcp_dnsservers",		{{ get_eth_dnsservers, NULL }}},

	/* for ppp if stats */
	{ "ppp_if_bytessent",	{{ get_eth_bytessent, NULL }}},
	{ "ppp_if_bytesreceived",	{{ get_eth_bytesreceived, NULL }}},
	{ "ppp_if_packetssent",	{{ get_eth_packetssent, NULL }}},
	{ "ppp_if_packetsreceived",	{{ get_eth_packetsreceived, NULL }}},
	{ "ppp_if_errorssent",	{{ get_eth_errorssent, NULL }}},
	{ "ppp_if_errorsreceived",	{{ get_eth_errorsreceived, NULL }}},
	{ "ppp_if_discardpacketsSent",	{{ get_eth_discardpacketssent, NULL }}},
	{ "ppp_if_discardpacketsreceived",	{{ get_eth_discardpacketsreceived, NULL }}},

	/* for ethernet if */
	{ "eth_if_enable",			{{ get_eth_enable, set_eth_enable }}},
	{ "eth_if_name",			{{ get_eth_name, NULL }}},
	{ "eth_if_macaddress",			{{ get_eth_macaddress, NULL }}},

	/* for usb if */
	{ "usb_if_enable",			{{ get_eth_enable, set_eth_enable }}},
	{ "usb_if_name",			{{ get_eth_name, NULL }}},
  
	/* for usb if stats */
	{ "usb_if_bytessent",	{{ get_eth_bytessent, NULL }}},
	{ "usb_if_bytesreceived",	{{ get_eth_bytesreceived, NULL }}},
	{ "usb_if_packetssent",	{{ get_eth_packetssent, NULL }}},
	{ "usb_if_packetsreceived",	{{ get_eth_packetsreceived, NULL }}},
	{ "usb_if_errorssent",	{{ get_eth_errorssent, NULL }}},
	{ "usb_if_errorsreceived",	{{ get_eth_errorsreceived, NULL }}},
	{ "usb_if_discardpacketsSent",	{{ get_eth_discardpacketssent, NULL }}},
	{ "usb_if_discardpacketsreceived",	{{ get_eth_discardpacketsreceived, NULL }}},	

	/* for bridging */
	//{ "bridging_port_name", 	 	{{ get_eth_port_name, NULL }}},

	/* for ipv6rd */
#if defined(RTCONFIG_IPV6) || defined(TCSUPPORT_IPV6)
	{ "ipv6rd_enable", 			{{ get_ipv6rd_enable, set_ipv6rd_enable}}},
#endif
	
	/* for ipv4forwarding of routing */
	{ "routing_ipv4forwarding",	{{ add_l3_forwarding, del_l3_forwarding }}},
	{ "routing_fw_destip",		{{ NULL, set_l3_config }}},
	{ "routing_fw_destnetmask",	{{ NULL, set_l3_config }}},
	{ "routing_fw_gatewayip",	{{ NULL, set_l3_config }}},
	{ "routing_fw_iface",		{{ NULL, set_l3_config }}},
	{ "routing_fw_metric",		{{ NULL, set_l3_config }}},

	/* for port mapping of nat */
	{ "nat_portmapping",			{{ add_eth_portmapping, del_eth_portmapping }}},
	{ "nat_pm_remotehost",			{{ NULL, set_port_mapping }}},
	{ "nat_pm_externalport",		{{ NULL, set_port_mapping }}},
	{ "nat_pm_externalportendrange",	{{ NULL, set_port_mapping }}},
	{ "nat_pm_internalport",		{{ NULL, set_port_mapping }}},
	{ "nat_pm_protocol",			{{ NULL, set_port_mapping }}},
	{ "nat_pm_internalclient",		{{ NULL, set_port_mapping }}},
	{ "nat_pm_description",			{{ NULL, set_port_mapping }}},

	/* for config of dhcpv4 client and server */
	{ "dhcpv4_client_enable",		{{ get_dhcpv4_client_enable, set_dhcpv4_client_enable }}},
	{ "dhcpv4_client_interface",		{{ get_dhcpv4_client_interface, set_dhcpv4_client_interface }}},
	{ "dhcpv4_client_status",		{{ get_dhcpv4_client_status, NULL }}},
	{ "dhcpv4_client_ipaddr",		{{ get_dhcpv4_client_ipaddr, NULL }}},
	{ "dhcpv4_client_subnetmask",		{{ get_dhcpv4_client_subnetmask, NULL }}},
	{ "dhcpv4_client_iprouters",		{{ get_dhcpv4_client_iprouters, NULL }}},
	{ "dhcpv4_client_dnsservers",		{{ get_dhcpv4_client_dnsservers, NULL }}},
	{ "dhcpv4_client_remaintime",		{{ get_dhcpv4_client_remaintime, NULL }}},

#ifdef DSL_ASUSWRT
	{ "dhcpv4_srv_enable",	{{ get_lanhost_dhcpserverenable, set_lanhost_dhcpserverenable }}},
#endif
	{ "dhcpv4_srv_dnsservers",	{{ get_lanhost_dnsservers, set_lanhost_dnsservers }}},
	{ "dhcpv4_srv_dhcpstatic",	{{ NULL, del_lanhost_dhcpstatic }}},
	{ "dhcpv4_srv_mac",	{{ NULL, set_lanhost_dhcpstatic }}},
	{ "dhcpv4_srv_ip",		{{ NULL, set_lanhost_dhcpstatic }}},

	/* for upnp */
	{ "upnp_enable", 			{{ get_upnpenable, set_upnpenable }}},
#endif

#ifdef TR232
	/* for bulkdata */
	{ "bulkdata_enable",	{{ NULL, set_bulkdata_enable }}},
	{ "bulkdata_status",	{{ get_bulkdata_status, NULL }}},
	{ "bulkdata_profile_enable",	{{ NULL, set_bulkdata_profile_enable }}},
	{ "bulkdata_profile",	{{ add_bulkdata_profile, del_bulkdata_profile }}},
	{ "profile_parameter",	{{ add_profile_parameter, del_profile_parameter }}},
#endif	

	/* for ASUS specfic */
	{ "cpuusage", 					{{ get_cpuusage,  NULL}}},
#ifdef ASUSWRT	
	{ "wan_connectiontype", 		{{ get_wan_connectiontype, set_wan_connectiontype }}},
	{ "wan_dhcpenable", 			{{ get_wan_dhcpenable, set_wan_dhcpenable }}},
	{ "wan_ip", 					{{ get_wan_ip, set_wan_ip }}},
	{ "wan_netmask", 				{{ get_wan_netmask, set_wan_netmask }}},
	{ "wan_gateway", 				{{ get_wan_gateway, set_wan_gateway }}},
	{ "wan_dnsenable", 				{{ get_wan_dnsenable, set_wan_dnsenable }}},
	{ "wan_dns1", 					{{ get_wan_dns1, set_wan_dns1 }}},
	{ "wan_dns2", 					{{ get_wan_dns2, set_wan_dns2 }}},
	{ "wan_username", 				{{ get_wan_username, set_wan_username }}},
	{ "wan_password", 				{{ get_wan_password, set_wan_password }}},
	{ "wan_vpnserver", 				{{ get_wan_vpnserver, set_wan_vpnserver }}},
	{ "wan_hostname", 				{{ get_wan_hostname, set_wan_hostname }}},	
#endif
	{ "primarywan", 				{{ get_primarywan, set_primarywan }}},	
	{ "secondarywan", 				{{ get_secondarywan, set_secondarywan }}},	
    	{ "user_passwd",		{{get_user_passwd, set_user_passwd }}},

#ifdef RTCONFIG_SFEXPRESS
	{ "ovpnc", 					{{ add_ovpnc,  del_ovpnc}}},
	{ "ovpnc_index", 			{{ get_ovpnc_index, set_ovpnc_index }}},
	{ "ovpnc_desc", 			{{ get_ovpnc_desc, set_ovpnc_desc }}},
	{ "ovpnc_username", 			{{ get_ovpnc_username, set_ovpnc_username }}},
	{ "ovpnc_password", 			{{ get_ovpnc_password, set_ovpnc_password }}},
	{ "ovpnc_autoreconn",		{{ get_ovpnc_autoreconn, set_ovpnc_autoreconn }}},
	{ "ovpnc_ovpnfile", 				{{ NULL, set_ovpnc_ovpnfile}}},
	{ "ovpnc_ca", 				{{ NULL, set_ovpnc_ca}}},
	{ "ovpnc_cert", 			{{ NULL, set_ovpnc_cert}}},
	{ "ovpnc_clientkey", 				{{ NULL, set_ovpnc_clientkey}}},
	{ "ovpnc_statickey", 			{{ NULL, set_ovpnc_statickey}}},
	{ "ovpnc_status", 			{{ get_ovpnc_status, NULL}}},
#endif	//RTCONFIG_SFEXPRESS

	{ NULL }
};

struct handler *get_handler(char *name)
{
	struct handler *handler = &handlers[0];

	while (handler && handler->name) {
		if (strcmp(handler->name, name) == 0)
			return handler;
		handler++;
	}

	return NULL;
}

void record_vendor_config_info(char *url)
{
	node_t *children = NULL;
	node_t node;
	int count = 0, rec_count = 0, need_add = 1;
	char buf[128] = {0}, dl_fname[128] = {0}, tmp[64] = {0};
	char *ptr = NULL;
	
	ptr = strrchr(url, '/');
	if(!ptr)
		return;		
	ptr++;
	snprintf(dl_fname, sizeof(dl_fname), "%s", ptr);
	tr_log(LOG_DEBUG, "url: %s, dl_fname: %s", url, dl_fname);

	if(lib_resolve_node(VENDOR_CONFIG_FILE, &node) == 0) {
		rec_count = count = lib_get_children(node, &children);
		while(count > 0) {
			char name[16] = {0};
			count--;

			if(lib_get_property(children[count], "name", name) == 0) {
				if(string_is_digits(name) == 1) {
					char fname[128];

					snprintf(buf, sizeof(buf), "%s.%s.Name", VENDOR_CONFIG_FILE, name);
					snprintf(fname, sizeof(fname), "%s", __get_parameter_value(buf, tmp));
					if(!strcmp(fname, dl_fname)) {
						snprintf(buf, sizeof(buf), "%s.%s.Date", VENDOR_CONFIG_FILE, name);
						snprintf(fname, sizeof(fname), "%s", __get_parameter_value(buf, tmp));
						__set_parameter_value(buf, lib_current_time());
						need_add = 0;
						break;
					}
				}
			}
		}

		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}
	
		/* add new info of vendor config file */
		if(need_add) {
			int i = 0;
			char count_str[8] = {0};

			snprintf(buf, sizeof(buf), "%s.", VENDOR_CONFIG_FILE);
			if((i = add_object(buf, strlen(buf))) >= 9000) {
				tr_log(LOG_ERROR, "add_object failed: %s", buf);
				return;
			}
			
			snprintf(buf, sizeof(buf), "%s.%d.Name", VENDOR_CONFIG_FILE, i);
			__set_parameter_value(buf, dl_fname);
			tr_log(LOG_DEBUG, "name: %s", dl_fname);

			snprintf(buf, sizeof(buf), "%s.%d.Date", VENDOR_CONFIG_FILE, i);
			__set_parameter_value(buf, lib_current_time());
			tr_log(LOG_DEBUG, "date: %s", lib_current_time());
			snprintf(count_str, sizeof(count_str), "%d", rec_count + 1);
			__set_parameter_value(VENDOR_CONFIG_FILE_NUM, count_str);
			tr_log(LOG_DEBUG, "count: %d", rec_count + 1);
		}
	}
}

int delete_all_instance(char *del_path)
{
	node_t *children = NULL;
	node_t node;
	int res = 1, count = 0;
	char nin[8]= {0}, path[256] = {0};

	if((res = lib_resolve_node(del_path, &node)) == 0) {
		count = lib_get_children(node, &children);

		while(count > 0) {
			char name[32] = {0};
			count--;

			if(lib_get_property(children[count], "name", name) == 0) {
				if(string_is_digits( name ) == 1) {
					sprintf(path, "%s.%s.", del_path, name);
					tr_log(LOG_DEBUG, "The path for delete: %s", path);
					delete_object(path, strlen(path));
				}
			}
		}

		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}

		war_snprintf(nin, sizeof( nin ), "1");
		lib_set_property(node, "nin", nin);	/* Reset nin property as 1 */
	}
	
	return res;
}

#ifdef TR232	/* start of TR232 */
static void bulkdata_profile_destroy(struct sched *sc)
{
	struct bulkdata_profile *bp;

	tr_log(LOG_DEBUG, "bulkdata_profile_destroy - start");
	bp = (struct bulkdata_profile *)(sc->pdata);

	if(bp) {
		free(bp);
		sc->pdata = NULL;
	}
	tr_log(LOG_DEBUG, "bulkdata_profile_destroy - end");
}

static void bulkdata_profile_timeout(struct sched *sc)
{
	struct bulkdata_profile *bp;
	time_t bp_tr = 0, cur = 0;

	tr_log(LOG_DEBUG, "bulkdata_profile_timeout - start");

	bp = (struct bulkdata_profile *)(sc->pdata);
	tr_log(LOG_DEBUG, "The path of profile: %s", bp->node_path);

	if(!generate_ipdrdoc(bp->node_path, bp->control_file_format, bp->count))
		bp->count++;

	/* set timeout */
	cur = current_time();
	if(strcasecmp(bp->time_reference, UNKNOWN_TIME) != 0 ) {
		if(string_time2second(bp->time_reference, &bp_tr) != 0) {
			time_t timeout = 0;
			
			timeout = ( bp_tr - cur ) % 3600;
			if(timeout > 0)
				sc->timeout = timeout + cur + bp->reporting_interval;
			else
				sc->timeout = timeout + cur + bp->reporting_interval + 3600;
		}
		else
			sc->timeout = cur + bp->reporting_interval;
	}
	else
		sc->timeout = cur + bp->reporting_interval;

	tr_log(LOG_DEBUG, "bulkdata_profile_timeout - end");
}

void add_profile_sched(char *path)
{
    char buf[128] = {0}, tmp[64] = {0};
	struct bulkdata_profile *bp = NULL;
	struct sched *sc = NULL;
	time_t bp_tr = 0, cur = 0;

	tr_log(LOG_DEBUG, "add_profile_sched: %s - start", path);

	bp = calloc(1, sizeof(*bp));
	if(bp == NULL) {
		tr_log(LOG_ERROR, "Out of memory!");
		return;
	}

	snprintf(bp->node_path, sizeof(bp->node_path), "%s", path);
	//bp->enable = 1;

	/* set alias */
	//snprintf(buf, sizeof(buf), "%s.Alias", path);
	//snprintf(bp->alias, sizeof(bp->alias), "%s", __get_parameter_value(buf));

	/* set protocol */
	//snprintf(buf, sizeof(buf), "%s.Protocol", path);
	//snprintf(bp->protocol, sizeof(bp->protocol), "%s", __get_parameter_value(buf));

	/* set encoding_type */
	//snprintf(buf, sizeof(buf), "%s.EncodingType", path);
	//snprintf(bp->encoding_type, sizeof(bp->encoding_type), "%s", __get_parameter_value(buf));

	/* set reporting_interval */
	snprintf(buf, sizeof(buf), "%s.ReportingInterval", path);
	bp->reporting_interval = strtoul(__get_parameter_value(buf, tmp), NULL, 10);

	/* set time_reference */
	snprintf(buf, sizeof(buf), "%s.TimeReference", path);
	snprintf(bp->time_reference, sizeof(bp->time_reference), "%s", __get_parameter_value(buf, tmp));

	/* set file_transfer_URL */
	snprintf(buf, sizeof(buf), "%s.FileTransferURL", path);
	snprintf(bp->file_transfer_URL, sizeof(bp->file_transfer_URL), "%s", __get_parameter_value(buf, tmp));

	/* set file_transfer_username */
	snprintf(buf, sizeof(buf), "%s.FileTransferUsername", path);
	snprintf(bp->file_transfer_username, sizeof(bp->file_transfer_username), "%s", __get_parameter_value(buf, tmp));

	/* set file_transfer_password */
	snprintf(buf, sizeof(buf), "%s.FileTransferPassword", path);
	snprintf(bp->file_transfer_password, sizeof(bp->file_transfer_password), "%s", __get_parameter_value(buf, tmp));

	/* set control_file_format */
	snprintf(buf, sizeof(buf), "%s.ControlFileFormat", path);
	snprintf(bp->control_file_format, sizeof(bp->control_file_format), "%s", __get_parameter_value(buf, tmp));

	sc = calloc(1, sizeof(*sc));
	if(sc == NULL) {
		tr_log(LOG_ERROR, "Out of memory!");
		free(bp);
		return;
	}

	sc->type = SCHED_WAITING_TIMEOUT;

	/* set timeout */
	cur = current_time();
	if(strcasecmp(bp->time_reference, UNKNOWN_TIME) != 0 ) {
		if(string_time2second(bp->time_reference, &bp_tr) != 0) {
			time_t timeout;
			
			timeout = ( bp_tr - cur ) % 3600;
			if(timeout > 0)
				sc->timeout = timeout + cur + bp->reporting_interval;
			else
				sc->timeout = timeout + cur + bp->reporting_interval + 3600;
		}
		else
			sc->timeout = cur + bp->reporting_interval;
	}
	else
		sc->timeout = cur + bp->reporting_interval;

	sc->fd = -2;	/* identify the profile of bulkdata for processing delete sched */
	sc->on_destroy = bulkdata_profile_destroy;
	sc->on_timeout = bulkdata_profile_timeout;
	sc->pdata = bp;
	add_sched(sc);
	tr_log(LOG_DEBUG, "add_profile_sched: %s - end", path);
}

void update_profile_sched(void)
{
	char buf[128] = {0}, bulkdata_enable[8] = {0}, tmp[64] = {0};
	node_t *children = NULL;
	node_t node;
	int count = 0;

	tr_log(LOG_DEBUG, "update_profile_sched - start");

	snprintf(buf, sizeof(buf), "%s", BULKDATA_ENABLE);
	snprintf(bulkdata_enable, sizeof(bulkdata_enable), "%s", __get_parameter_value(buf, tmp));

	if((!strcasecmp(bulkdata_enable, "true") || !strcasecmp(bulkdata_enable, "1")) &&
			 (lib_resolve_node(BULKDATA_PROFILE, &node) == 0)) {
		count = lib_get_children(node, &children);
		while(count > 0) {
			char name[16] = {0};
			count--;

			if(lib_get_property(children[count], "name", name) == 0) {
				if(string_is_digits(name) == 1) {
					char profile_enable[8];

					snprintf(buf, sizeof(buf), "%s.%s.Enable", BULKDATA_PROFILE, name);
					snprintf(profile_enable, sizeof(profile_enable), "%s", __get_parameter_value(buf, tmp));
	
					if(!strcasecmp(profile_enable, "true") || !strcasecmp(profile_enable, "1")) {	/* add sched */
						char path[128];

						snprintf(path, sizeof(path), "%s.%s", BULKDATA_PROFILE, name);
						add_profile_sched(path);
					}
				}
			}
		}

		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}
	}

	tr_log(LOG_DEBUG, "update_profile_sched - end");
}

static int set_bulkdata_profile_enable(node_t node, char *arg, char *value)
{
	char buf[128] = {0}, bulkdata_enable[8] = {0}, tmp[64] = {0};

	snprintf(buf, sizeof(buf), "%s", BULKDATA_ENABLE);
	snprintf(bulkdata_enable, sizeof(bulkdata_enable), "%s", __get_parameter_value(buf, tmp));

	if(!strcasecmp(bulkdata_enable, "true") || !strcasecmp(bulkdata_enable, "1")) {
		char *path = lib_node2path(node->parent);
	
		path[strlen(path) - 1] = '\0';
	 
		if(!strcasecmp(value, "true") || !strcasecmp(value, "1"))	/* add sched */
			add_profile_sched(path);
		else	/* del sched */
			del_profile_sched(path);
	}

	return 0;
}

static int set_bulkdata_enable(node_t node, char *arg, char *value)
{
	char buf[128] = {0}, tmp[64] = {0};
	node_t *children = NULL;
	node_t profile_node;
	int count = 0;

	/* delete all sched for profile of bulkdata */
	if(lib_resolve_node(BULKDATA_PROFILE, &profile_node) == 0) {
		count = lib_get_children(profile_node, &children);
		while(count > 0) {
			char name[16] = {0};
			count--;

			if(lib_get_property(children[count], "name", name) == 0) {
				if(string_is_digits(name) == 1) {
					char profile_enable[8];

					snprintf(buf, sizeof(buf), "%s.%s.Enable", BULKDATA_PROFILE, name);
					snprintf(profile_enable, sizeof(profile_enable), "%s", __get_parameter_value(buf, tmp));
	
					if(!strcasecmp(profile_enable, "true") || !strcasecmp(profile_enable, "1")) {	/* del sched */
						char path[128];

						snprintf(path, sizeof(path), "%s.%s", BULKDATA_PROFILE, name);
						del_profile_sched(path);
					}
				}
			}
		}

		if(children) {
			lib_destroy_children(children);
			children = NULL;
		}
	}

	/* update sched for profile of bulkdata */
	if(!strcasecmp(value, "true") || !strcasecmp(value, "1"))
		update_profile_sched();

	return 0;
}

static int get_bulkdata_status(node_t node, char *arg, char **value)
{
	char buf[32] = {0}, enable[8] = {0}, tmp[64] = {0};

	snprintf(enable, sizeof(enable), "%s", __get_parameter_value(BULKDATA_ENABLE, tmp));
	if(!strcasecmp(enable, "true") || !strcasecmp(enable, "1"))
		snprintf(buf, sizeof(buf), "Enabled");
	else
		snprintf(buf, sizeof(buf), "Disabled");
	
	*value = strdup(buf);
	return *value ? 0 : -1;
}

static int count_bulkdata_profile(char *path, int need_add)
{
	int count = 0;
	char count_str[8] = {0}, tmp[64] = {0};

	if(need_add)
		count = atoi(__get_parameter_value(path, tmp)) + 1;
	else
		count = atoi(__get_parameter_value(path, tmp)) - 1;
	snprintf(count_str, sizeof(count_str), "%d", count);
	__set_parameter_value(path, count_str);
	
	return 0;
}

static int add_bulkdata_profile(node_t node, char *arg, int nin)
{
	char buf[128] = {0};

	snprintf(buf, sizeof(buf), "%s.ProfileNumberOfEntries", BULKDATA);
	return count_bulkdata_profile(buf, 1);
}

static int del_bulkdata_profile(node_t node, char *arg, int nin)
{
	char buf[128] = {0};

	snprintf(buf, sizeof(buf), "%s.ProfileNumberOfEntries", BULKDATA);
	return count_bulkdata_profile(buf, 0);	
}

static int add_profile_parameter(node_t node, char *arg, int nin)
{
	char buf[128] = {0};
	char *path = lib_node2path(node->parent);
	
	path[strlen(path) - 1] = '\0';
	snprintf(buf, sizeof(buf), "%s.ParameterNumberOfEntries", path);
	//tr_log(LOG_DEBUG, "add_profile_parameter: %s", path);
	return count_bulkdata_profile(buf, 1);
}

static int del_profile_parameter(node_t node, char *arg, int nin)
{
	char buf[128] = {0};
	char *path = lib_node2path(node->parent);

	path[strlen(path) - 1] = '\0';
	snprintf(buf, sizeof(buf), "%s.ParameterNumberOfEntries", path);
	//tr_log(LOG_DEBUG, "del_profile_parameter: %s", path);
	return count_bulkdata_profile(buf, 0);
}
#endif	/* end of TR232 */
