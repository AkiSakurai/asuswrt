export SRCBASE := $(shell pwd)
export SRCBASEDIR := $(shell pwd | sed 's/.*release\///g')
export HND_SRC := $(shell pwd | sed 's/\(.*src-rt-.*hnd.*\).*/\1/')
ifeq ($(HND_ROUTER_AX),)
export HND_ROUTER_AX := $(shell pwd | sed 's/.*axhnd.*/y/g')
endif

include ./platform.mak
include $(SRCBASE)/router/common.mak
include upb.inc
-include .config
-include $(LINUXDIR)/.config
-include $(SRCBASE)/router/.config

ifeq ($(ASUSWRTTARGETMAKDIR),)
include ./target.mak
else
include $(ASUSWRTTARGETMAKDIR)/target.mak
endif

ifeq ($(ASUSWRTVERSIONCONFDIR),)
include ./version.conf
else
include $(ASUSWRTVERSIONCONFDIR)/version.conf
endif

-include $(SRCBASE)/router/extendno.conf

ifeq ($(BUILD_NAME),)
$(error BUILD_NAME is not defined.!!!)
endif

#################################################################################################
# If SRCBASE = /PATH/TO/asuswrt/router/src-ra-3.0, PBPREFIX is not defined,
#    BUILD_NAME=RT-N65U, and firmware version is 3.0.0.4.308 thus
#
# SRCPREFIX:	/PATH/TO/asuswrt/release
# TOPPREFIX:	/PATH/TO/asuswrt/release/src/router
# PBDIR:	/PATH/TO/asuswrt.prebuilt/RT-N65U.3.0.0.4.308
# PBPREFIX:	/PATH/TO/asuswrt.prebuilt/RT-N65U.3.0.0.4.308/release
# PBSRCDIR:	/PATH/TO/asuswrt.prebuilt/RT-N65U.3.0.0.4.308/release/src-ra-3.0
# PBTOPDIR:	/PATH/TO/asuswrt.prebuilt/RT-N65U.3.0.0.4.308/release/src/router
# PBLINUXDIR:	/PATH/TO/asuswrt.prebuilt/RT-N65U.3.0.0.4.308/release/src-ra-3.0/linux/linux-3.x
#################################################################################################
export SRCPREFIX = $(shell pwd | sed 's,^\(.*release/\)src.*,\1,')
export PLATFORMSRC = $(shell pwd | sed 's,^.*release/\(src.*\),\1,')
export TOPPREFIX = $(shell pwd | sed 's,^\(.*release/\)src.*,\1,')/src/router

ifneq ($(wildcard $(shell pwd)/router-sysdep),)
export TOP_PLATFORM := $(shell pwd)/router-sysdep
else
export TOP_PLATFORM := $(shell pwd)/router
endif

ifeq ($(PBDIR),)
export PBDIR    = $(abspath $(SRCBASE)/../../../asuswrt.prebuilt/$(BUILD_NAME).$(KERNEL_VER).$(FS_VER).$(SERIALNO).$(EXTENDNO))
export PBPREFIX = $(abspath $(SRCBASE)/../../../asuswrt.prebuilt/$(BUILD_NAME).$(KERNEL_VER).$(FS_VER).$(SERIALNO).$(EXTENDNO)/release)
else
export PBPREFIX = $(abspath $(PBDIR)/release)
endif
export PBSRCDIR = $(PBPREFIX)/$(SRCBASEDIR)
export PBTOPDIR = $(PBPREFIX)/src/router
export SYSDEP_DIR = $(PBPREFIX)/src/router-sys
export PBLINUXDIR := $(PBPREFIX)/$(SRCBASEDIR)/linux/$(shell basename $(LINUXDIR))
# related path to platform specific software package
export PLATFORM_TOPDIR = $(SRCBASE)/$(PLATFORM_ROUTER)
export PLATFORM_PBTOPDIR = $(PBSRCDIR)/$(PLATFORM_ROUTER)

pb-$(RTCONFIG_WEBDAV)                 += lighttpd-1.4.39
pb-$(RTCONFIG_CLOUDSYNC)              += asuswebstorage
pb-$(RTCONFIG_USB_BECEEM)             += Beceem_BCMS250
pb-$(CONFIG_USB_BECEEM)               += Beceem_driver
pb-$(RTCONFIG_CLOUDSYNC)              += inotify
pb-$(RTCONFIG_PERMISSION_MANAGEMENT)  += PMS_DBapis
pb-$(RTCONFIG_SWEBDAVCLIENT)          += webdav_client
pb-$(RTCONFIG_FBWIFI)                 += fb_wifi
pb-$(RTCONFIG_FBWIFI)                 += httpd_uam
pb-y                                  += extendno
pb-y                                  += rc
pb-y                                  += httpd
pb-y                                  += shared
pb-y                                  += wb
pb-y                                  += networkmap
pb-$(RTCONFIG_TUNNEL)                 += aaews
pb-$(RTCONFIG_TUNNEL)                 += asusnatnl
pb-$(RTCONFIG_USB_PRINTER)            += u2ec
pb-$(RTCONFIG_DROPBOXCLIENT)          += dropbox_client
pb-$(RTCONFIG_FTPCLIENT)              += ftpclient
pb-$(RTCONFIG_SAMBACLIENT)            += sambaclient
pb-$(RTCONFIG_USBCLIENT)              += usbclient
pb-$(RTCONFIG_QUICKSEC)               += quicksec-6.0
pb-$(RTCONFIG_USB_SMS_MODEM)          += smspdu
pb-$(RTCONFIG_DSL)                    += spectrum
pb-$(RTCONFIG_DSL)                    += dsl_drv_tool
pb-$(RTCONFIG_BWDPI)                  += bwdpi_source
pb-$(RTCONFIG_OPENVPN)                += libvpn
pb-$(RTCONFIG_CFGSYNC)                += cfg_mnt
pb-y                                  += sysstate
pb-$(RTCONFIG_NOTIFICATION_CENTER)    += nt_center
pb-$(RTCONFIG_NOTIFICATION_CENTER)    += wlc_nt
pb-$(RTCONFIG_PROTECTION_SERVER)      += protect_srv
pb-$(RTCONFIG_RGBLED)                 += aura_sw
pb-$(RTCONFIG_SW_HW_AUTH)             += sw-hw-auth
pb-$(RTCONFIG_LIBASUSLOG)             += libasuslog
pb-$(RTCONFIG_INTERNAL_GOBI)          += usb-gobi
pb-$(RTCONFIG_BT_CONN)                += bluez-5.41
pb-$(RTCONFIG_DBLOG)                  += dblog
pb-$(RTCONFIG_AMAS)                   += amas-utils
pb-$(RTCONFIG_LETSENCRYPT)            += libletsencrypt
pb-$(RTCONFIG_AHS)                  += ahs
pb-$(RTCONFIG_LIBASC)                  += libasc
pb-$(RTCONFIG_ASD)                  += asd

$(info SRCPREFIX $(SRCPREFIX))
$(info PBPREFIX $(PBPREFIX))

ALL_FILES = $(shell find $(PBPREFIX) \( -path "$(PBPREFIX)/src/router/Beceem_BCMS250/prebuild/rom" \) -prune -o -type f -print)
SO_FILES = $(filter %.so %.so.l %/libxvi020.so.05.02.93, $(ALL_FILES))
SH_FILES = $(filter %.sh, $(ALL_FILES))
OBJ_FILES = $(filter %.o %.obj, $(ALL_FILES))
KO_FILES = $(filter %.ko, $(ALL_FILES))
TXT_FILES = $(filter %.conf %.txt, $(ALL_FILES))
SRC_FILES = $(filter %.h %.c, $(ALL_FILES))
MAKEFILES = $(shell find $(PBTOPDIR) -name Makefile) $(filter %.mak, $(ALL_FILES))
TOOLSFILES = $(filter %/trx_asus %/addvtoken, $(ALL_FILES))
EXEC_FILES = $(filter-out $(SO_FILES) $(OBJ_FILES) $(KO_FILES) $(TXT_FILES) $(SRC_FILES) $(MAKEFILES) $(TOOLSFILES) $(SH_FILES) $(SYSDEP_DIR), $(ALL_FILES))

all: $(pb-y) $(pb-m)
	[ ! -e upb-platform.mak ] || $(MAKE) -f upb-platform.mak
	$(MAKE) -f upb.mak strip

strip:
	-@( \
		for f in $(SO_FILES) $(OBJ_FILES) $(KO_FILES) ; do \
			if [ -z "$(V)" ] ; then echo "  [STRIP]  `basename $${f}`" ; \
			else echo "$(STRIP) --strip-debug $${f}" ; fi ;\
			$(STRIP) --strip-debug $${f} ; \
		done ; \
		for f in $(EXEC_FILES) ; do \
			if [ -z "$(V)" ] ; then echo "  [STRIP ALL]  `basename $${f}`" ; \
			else echo "$(STRIP) --strip-all $${f}" ; fi ;\
			$(STRIP) --strip-all $${f} ; \
		done \
	 )

clean:
	-$(RM) -fr $(PBDIR)

############################################################################
# Generic short variable for source/destination directory.
# NOTE: Only one variable can be defined in one target.
############################################################################
$(pb-y) $(pb-m): S=$(TOPPREFIX)/$@
$(pb-y) $(pb-m): D=$(PBTOPDIR)/$@/prebuild


############################################################################
# Override special S or D variable here.
# NOTE: Only one variable can be defined in one target.
############################################################################
lighttpd-1.4.39: S=$(TOPPREFIX)/$@/src/.libs
Beceem_BCMS250: S=$(TOPPREFIX)/$@/src
Beceem_driver: S=$(LINUXDIR)/drivers/usb/$@
Beceem_driver: D=$(PBTOPDIR)/Beceem_BCMS250/prebuild/lib/modules/$(LINUX_KERNEL)$(BCMSUB)/kernel/drivers/usb/$@
extendno: S=$(TOPPREFIX)/
extendno: D=$(PBTOPDIR)/
quicksec-6.0: S=$(TOPPREFIX)/$@/build/linux
bwdpi_source: D=$(PBTOPDIR)/$@/
sysstate: D=$(PBTOPDIR)/$@/
dblog: D=$(PBTOPDIR)/$@/
nt_center: D=$(PBTOPDIR)/$@/
protect_srv: D=$(PBTOPDIR)/$@/
sw-hw-auth: S=$(TOPPREFIX)/shared/

############################################################################
# Copy binary to prebuilt directory.
############################################################################
asuswebstorage:
	$(call inst,$(S),$(D),$@)

Beceem_BCMS250: C=$(S)/CSCM_v1.1.6.0
Beceem_BCMS250: T=$(TOOLS)
Beceem_BCMS250: RT_VER=$(RTVER)
Beceem_BCMS250:
	$(call _inst,$(S)/bcmWiMaxSwitch/switchmode,$(D)/sbin/switchmode)
	$(call _inst,$(S)/API/bin_linux/bin/libxvi020.so.05.02.93,$(D)/lib/libxvi020.so.05.02.93)
	$(call _ln,libxvi020.so.05.02.93,$(D)/lib/libxvi020.so)
	$(call _inst,$(C)/bin_pc_linux/bin/wimaxc,$(D)/sbin/wimaxc)
	$(call _inst,$(C)/bin_pc_linux/bin/wimaxd,$(D)/sbin/wimaxd)
	$(call _inst,$(C)/bin_pc_linux/bin/libeap_supplicant.so,$(D)/lib/libeap_supplicant.so)
	$(call _inst,$(C)/bin_pc_linux/bin/libengine_beceem.so,$(D)/lib/libengine_beceem.so)
	$(call _inst,$(T)/lib/librt-$(RT_VER).so,$(D)/lib/librt-$(RT_VER).so)
	$(call _ln,librt-$(RT_VER).so,$(D)/lib/librt.so)
	$(call _ln,librt-$(RT_VER).so,$(D)/lib/librt.so.0)
	$(call _ln,/tmp/Beceem_firmware,$(D)/lib/firmware)
	$(call _inst,$(S)/API/bin_linux/bin/RemoteProxy.cfg,$(D)/rom/Beceem_firmware/RemoteProxy.cfg)
	$(call _inst,$(S)/macxvi200.bin.normal,$(D)/rom/Beceem_firmware/macxvi200.bin.normal)
	$(call _inst,$(S)/macxvi200.bin.giraffe,$(D)/rom/Beceem_firmware/macxvi200.bin.giraffe)
	$(call _inst,$(S)/config_files/macxvi.cfg.giraffe,$(D)/rom/Beceem_firmware/macxvi.cfg.giraffe)
	$(call _inst,$(S)/config_files/macxvi.cfg.gmc,$(D)/rom/Beceem_firmware/macxvi.cfg.gmc)
	$(call _inst,$(S)/config_files/macxvi.cfg.yota,$(D)/rom/Beceem_firmware/macxvi.cfg.yota)
	$(call _inst,$(S)/config_files/macxvi.cfg.freshtel,$(D)/rom/Beceem_firmware/macxvi.cfg.freshtel)
	$(call _inst,$(S)/config_files/Server_CA.pem.yota,$(D)/rom/Beceem_firmware/Server_CA.pem.yota)

Beceem_driver:
	$(call inst,$(S),$(D),drxvi314.ko)

dropbox_client:
	$(call inst,$(S),$(D),dropbox_client)

ftpclient:
	$(call inst,$(S),$(D),ftpclient)

inotify:
	$(call inst,$(S),$(D),$@)

lighttpd-1.4.39:
	$(call inst,$(S),$(D),mod_aicloud_auth.so)
	$(call inst,$(S),$(D),mod_aicloud_invite.so)
	$(call inst,$(S),$(D),mod_aicloud_sharelink.so)
	$(call inst,$(S),$(D),mod_aidisk_access.so)
	$(call inst,$(S),$(D),mod_captive_portal_uam.so)
	$(call inst,$(S),$(D),mod_create_captcha_image.so)
	$(call inst,$(S),$(D),mod_query_field_json.so)
	$(call inst,$(S),$(D),mod_smbdav.so)

PMS_DBapis:
	$(call inst,$(S),$(D),libpms_sql.so)

quicksec-6.0:
	$(call inst,$(S),$(D),interceptor/quicksec.ko)
	$(call inst,$(S),$(D),usermode/quicksecpm)

webdav_client:
	$(call inst,$(S),$(D),$@)

fb_wifi:
	$(call inst,$(S),$(D),fb_wifi_register)
	$(call inst,$(S),$(D),fb_wifi_check)
	$(call inst_so,$(S),$(D),libfbwifi.so)

httpd_uam:
	$(call inst,$(S),$(D),httpd_uam)

extendno:
	$(call inst,$(S),$(D),extendno.conf)

rc:
	$(call inst,$(S),$(D),private.o)
ifeq ($(RTCONFIG_CONNDIAG),y)
	$(call inst,$(S),$(D),conn_diag.o)
	$(call inst,$(S),$(D),conn_diag-sql.o)
endif
ifeq ($(RTCONFIG_DSL),y)
ifeq ($(RTCONFIG_DSL_TCLINUX),y)
	$(call inst,$(S),$(D),dsl_fb.o)
	$(call inst,$(S),$(D),dsl_diag.o)
endif
endif
ifeq ($(RTCONFIG_FRS_FEEDBACK),y)
	$(call inst,$(S),$(D),dsl_fb.o)
ifeq ($(RTCONFIG_DBLOG),y)
	$(call inst,$(S),$(D),dblog.o)
endif
endif
ifeq ($(RTCONFIG_TAGGED_BASED_VLAN),y)
	$(call inst,$(S),$(D),tagged_based_vlan.o)
endif
ifeq ($(RTCONFIG_HTTPS),y)
	$(call inst,$(S),$(D),pwdec.o)
endif
ifeq ($(RTCONFIG_TCODE),y)
	$(call inst,$(S),$(D),tcode_rc.o)
endif
ifeq ($(CONFIG_BCMWL5),y)
	$(call inst,$(S),$(D),tcode_brcm.o)
	$(call inst,$(S),$(D),ate-broadcom.o)
ifeq ($(BUILD_NAME),$(filter $(BUILD_NAME),RT-AC68U RT-AC3200 RT-AX58U TUF-AX3000 RT-AX82U))
	$(call inst,$(S),$(D),cfe.o)
endif
ifeq ($(RTCONFIG_ADTBW),y)
	$(call inst,$(S),$(D),adtbw-broadcom.o)
endif
ifeq ($(RTCONFIG_AMAS_ADTBW),y)
	$(call inst,$(S),$(D),amas_adtbw.o)
	$(call inst,$(S),$(D),adtbw-broadcom.o)
endif
ifeq ($(and $(RTCONFIG_BCMWL6),$(RTCONFIG_PROXYSTA)),y)
	$(call inst,$(S),$(D),psta_monitor.o)
endif
ifeq ($(RTCONFIG_USER_LOW_RSSI),y)
ifeq ($(RTCONFIG_BCMWL6), y)
	$(call inst,$(S),$(D),roamast-broadcom.o)
ifeq ($(RTCONFIG_CONNDIAG),y)
	$(call inst,$(S),$(D),conn_diag-broadcom.o)
endif
endif
endif
	$(call inst,$(S),$(D),broadcom.o)
endif

ifeq ($(RTCONFIG_RALINK),y)
	$(call inst,$(S),$(D),ate-ralink.o)
	$(call inst,$(S),$(D),air_monitor.o)
	$(call inst,$(S),$(D),ralink.o)
	$(call inst,$(S),$(D),roamast-ralink.o)
endif

ifeq ($(RTCONFIG_REALTEK),y)
	$(call inst,$(S),$(D),ate-realtek.o)
	$(call inst,$(S),$(D),realtek.o)
	$(call inst,$(S),$(D),rtk_wifi_drvmib.o)
endif

ifeq ($(RTCONFIG_QCA),y)
	$(call inst,$(S),$(D),ate-qca.o)
	$(call inst,$(S),$(D),ctl.obj)
endif

ifeq ($(RTCONFIG_SPEEDTEST),y)
	$(call inst,$(S),$(D),speedtest.o)
endif

ifeq ($(RTCONFIG_LACP),y)
	$(call inst,$(S),$(D),agg_brcm.o)
endif

ifeq ($(RTCONFIG_NOTIFICATION_CENTER),y)
	$(call inst,$(S),$(D),nt_mail.o)
endif

ifeq ($(RTCONFIG_ALPINE),y)
	$(call inst,$(S),$(D),alpine.o)
	$(call inst,$(S),$(D),ate-alpine.o)
	$(call inst,$(S),$(D),init-alpine.o)
	$(call inst,$(S),$(D),qsr10g.o)
endif
ifeq ($(RTCONFIG_BWDPI),y)
	$(call inst,$(S),$(D),bwdpi.o)
	$(call inst,$(S),$(D),bwdpi_check.o)
	$(call inst,$(S),$(D),bwdpi_wred_alive.o)
	$(call inst,$(S),$(D),bwdpi_db_10.o)
endif

ifeq ($(RTCONFIG_LANTIQ),y)
	$(call inst,$(S),$(D),lantiq.o)
	$(call inst,$(S),$(D),ate-lantiq.o)
	$(call inst,$(S),$(D),init-lantiq.o)
	$(call inst,$(S),$(D),lantiq-wave.o)
	$(call inst,$(S),$(D),obd-lantiq.o)
	$(call inst,$(S),$(D),wps-lantiq.o)
	$(call inst,$(S),$(D),roamast-lantiq.o)
	$(call inst,$(S),$(D),client.o)
endif

ifeq ($(RTCONFIG_AMAS),y)
	$(call inst,$(S),$(D),obd.o)
	$(call inst,$(S),$(D),obd_eth.o)
	$(call inst,$(S),$(D),obd_monitor.o)
endif

ifeq ($(RTCONFIG_NEW_USER_LOW_RSSI),y)
	$(call inst,$(S),$(D),roamast.o)
endif

ifeq ($(RTCONFIG_TR069),y)
	$(call inst,$(S),$(D),tr069.o)
endif

ifeq ($(RTCONFIG_AMAS),y)
	$(call inst,$(S),$(D),amas_wlcconnect.o)
	$(call inst,$(S),$(D),amas_bhctrl.o)
	$(call inst,$(S),$(D),amas_lanctrl.o)
	$(call inst,$(S),$(D),amas_lib.o)
endif
ifeq ($(RTCONFIG_FRS_LIVE_UPDATE),y)
	$(call inst,$(S),$(D),frs_service.o)
endif

ifeq ($(RTCONFIG_FRS_LIVE_UPDATE),y)
	$(call inst,$(S),$(D),frs_service.o)
endif

httpd:
	$(call inst,$(S),$(D),web_hook.o)
ifeq ($(RTCONFIG_HTTPS),y)
	$(call inst,$(S),$(D),pwenc.o)
endif
ifeq ($(RTCONFIG_LANTIQ), y)
	$(call inst,$(S),$(D),web-lantiq.o)
endif
ifeq ($(RTCONFIG_BRCM_HOSTAPD),y)
	$(call inst,$(S),$(D),wps_pbcd.o)
	$(call inst,$(S),$(D),hostapd_config.o)
endif
ifeq ($(BUILD_NAME),$(filter $(BUILD_NAME),RT-AX82U))
	$(call inst,$(S),$(D),ledg.o)
endif

spectrum:
	$(call inst,$(S),$(D),$@)

dsl_drv_tool:
	$(call inst,$(S)/adslate,$(D)/../adslate/prebuild,adslate)
	$(call inst,$(S)/auto_det,$(D)/../auto_det/prebuild,auto_det)
	$(call inst,$(S)/req_dsl_drv,$(D)/../req_dsl_drv/prebuild,req_dsl_drv)
	$(call inst,$(S)/tp_init,$(D)/../tp_init/prebuild,tp_init)

sambaclient:
	$(call inst,$(S),$(D),sambaclient)

shared:
	$(call inst,$(S),$(D),notify_rc.o)
ifeq ($(RTCONFIG_AHS),y)
	$(call inst,$(S),$(D),notify_ahs.o)
endif
ifeq ($(RTCONFIG_TCODE),y)
	$(call inst,$(S),$(D),tcode.o)
endif
ifeq ($(or $(RTCONFIG_ALPINE),$(CONFIG_BCMWL5),$(RTCONFIG_REALTEK),$(RTCONFIG_RALINK),$(RTCONFIG_LANTIQ),$(RTCONFIG_QCA)),y)
	$(call inst,$(S),$(D),private.o)
endif
	$(call inst,$(S),$(D),shutils_private.o)
ifeq ($(RTCONFIG_RGBLED),y)
	$(call inst,$(S),$(D),aura_sync.o)
endif

	$(call inst,$(S),$(D),spwenc.o)

ifeq ($(or $(RTCONFIG_RALINK),$(RTCONFIG_QCA)),y)
	$(call inst,$(S),$(D),private.o)
endif

ifeq ($(RTCONFIG_SAVEJFFS),y)
	$(call inst,$(S),$(D),jffs_cfgs.o)
endif
ifeq ($(RTCONFIG_LANTIQ), y)
	$(call inst,$(S),$(D),lantiq.o)
	$(call inst,$(S),$(D),api-lantiq.o)
endif

ifeq ($(RTCONFIG_DWB),y)
	$(call inst,$(S),$(D),amas_dwb.o)
endif

ifeq ($(RTCONFIG_AMAS),y)
	$(call inst,$(S),$(D),amas_utils.o)
endif

usbclient:
	$(call inst,$(S),$(D),usbclient)

wb:
	$(call inst,$(S),$(D),libws.so)

aaews:
	$(call inst,$(S),$(D),aaews)
	$(call inst,$(S),$(D),mastiff)

asusnatnl:
	$(call inst,$(S)/natnl,$(D)/../natnl/prebuild,libasusnatnl.so)

u2ec:
	$(call inst,$(S),$(D),u2ec)

smspdu:
	$(call inst,$(S),$(D),libsmspdu.so)
	$(call inst,$(S),$(D),pullsms)
	$(call inst,$(S),$(D),smspdu)

bwdpi_source:
	$(call inst,$(S)/asus/,$(D)/asus/prebuild,libbwdpi.so)
	$(call inst,$(S)/asus/,$(D)/asus/prebuild,hwinfo)
	$(call inst,$(S)/include/tdts/,$(D)/prebuild,tmcfg.h)
	$(call inst,$(S)/include/udb/,$(D)/prebuild,tmcfg_udb.h)
	$(call inst,$(S)/asus_sql/,$(D)/asus_sql/prebuild,libbwdpi_sql.so)
	$(call inst,$(S)/asus_sql/,$(D)/asus_sql/prebuild,bwdpi_sqlite)
	$(call inst,$(S)/RC_INDEP/,$(D)/prebuild,dcd)
	$(call inst,$(S)/RC_INDEP/,$(D)/prebuild,libshn_pctrl.so)
	$(call inst,$(S)/RC_INDEP/,$(D)/prebuild,libshn_utils.so)
	$(call inst,$(S)/RC_INDEP/,$(D)/prebuild,ntdasus2014.cert)
	$(call inst,$(S)/RC_INDEP/,$(D)/prebuild,rule.trf)
	$(call inst,$(S)/RC_INDEP/,$(D)/prebuild,sample.bin)
	$(call inst,$(S)/RC_INDEP/,$(D)/prebuild,shn_ctrl)
	$(call inst,$(S)/RC_INDEP/,$(D)/prebuild,tcd)
	$(call inst,$(S)/RC_INDEP/,$(D)/prebuild,tdts.ko)
	$(call inst,$(S)/RC_INDEP/,$(D)/prebuild,tdts_rule_agent)
	$(call inst,$(S)/RC_INDEP/,$(D)/prebuild,tdts_udbfw.ko)
	$(call inst,$(S)/RC_INDEP/,$(D)/prebuild,tdts_udb.ko)
	$(call inst,$(S)/RC_INDEP/,$(D)/prebuild,wred)
	$(call inst,$(S)/RC_INDEP/,$(D)/prebuild,wred_set_conf)
	$(call inst,$(S)/RC_INDEP/,$(D)/prebuild,wred_set_wbl)

sysstate:
	$(call inst,$(S)/log_daemon/,$(D)/log_daemon/prebuild,sysstate)
	$(call inst,$(S)/commands/,$(D)/commands/prebuild,asuslog)

dblog:
	$(call inst,$(S)/daemon/,$(D)/daemon/prebuild,dblog)
	$(call inst,$(S)/commands/,$(D)/commands/prebuild,dblogcmd)

libvpn:
	$(call inst,$(S),$(D),libvpn.so)

cfg_mnt:
	$(call inst,$(S),$(D),cfg_client)
	$(call inst,$(S),$(D),cfg_server)
	$(call inst,$(S),$(D),cfg_reportstatus)
	$(call inst,$(S),$(D),libcfgmnt.so)

nt_center:
	$(call inst,$(S),$(D)/prebuild,Notify_Event2NC)
	$(call inst,$(S),$(D)/prebuild,nt_center)
	$(call inst,$(S),$(D)/prebuild,nt_monitor)
	$(call inst,$(S)/actMail,$(D)/actMail/prebuild,nt_actMail)
	$(call inst,$(S)/lib,$(D)/lib/prebuild,libnt.so)
	$(call inst,$(S)/lib,$(D)/lib/prebuild,nt_db)

wlc_nt:
	$(call inst,$(S),$(D),wlc_nt)
	$(call inst,$(S),$(D),libwlc_nt_client.so)

protect_srv:
	$(call inst,$(S),$(D)/prebuild,protect_srv)
	$(call inst,$(S),$(D)/prebuild,Send_Event2ptcsrv)
	$(call inst,$(S)/lib,$(D)/lib/prebuild,libptcsrv.so)
	$(call inst,$(S),$(D)/prebuild,req_ptcsrv)

networkmap:
	$(call inst,$(S),$(D),networkmap)
	-$(call inst,$(S),$(D),asusdiscovery)

aura_sw:
	$(call inst,$(S),$(D),sb_flash_update)

libletsencrypt:
	$(call inst,$(S),$(D),libletsencrypt.so)

libasuslog:
	$(call inst,$(S),$(D),libasuslog.so)

sw-hw-auth:
	$(call inst,$(S),$(D),sw_auth.o)
	$(call inst,$(S),$(D),hw_auth.o)

usb-gobi:
	$(call inst,$(S)/kernel_module/gobi,$(D),gobi.ko)
	$(call inst,$(S)/src/lib,$(D),libGobiConnectionMgmt.00.00.05.so)
	$(call inst,$(S)/src,$(D),gobi_api)
	$(call inst,$(S)/src,$(D),sample)

bluez-5.41:
	$(call inst,$(S)/tools,$(D),bccmd)
	$(call inst,$(S)/tools,$(D),bluemoon)
	$(call inst,$(S)/tools,$(D),ciptool)
	$(call inst,$(S)/tools,$(D),hcitool)
	$(call inst,$(S)/tools,$(D),hcidump)
	$(call inst,$(S)/tools,$(D),hciattach)
	$(call inst,$(S)/tools,$(D),hciconfig)
	$(call inst,$(S)/tools,$(D),hid2hci)
	$(call inst,$(S)/tools,$(D),l2ping)
	$(call inst,$(S)/tools,$(D),l2test)
	$(call inst,$(S)/tools,$(D),mpris-proxy)
	$(call inst,$(S)/tools,$(D),rctest)
	$(call inst,$(S)/tools,$(D),rfcomm)
	$(call inst,$(S)/tools,$(D),sdptool)
	$(call inst,$(S)/tools,$(D),btmgmt)
	$(call inst,$(S)/client,$(D),bluetoothctl)
	$(call inst,$(S)/src,$(D),bluetoothd)
	$(call inst,$(S)/attrib,$(D),gatttool)
	$(call inst,$(S)/lib/.libs,$(D),libbluetooth.so.3.18.13)

amas-utils:
	$(call inst,$(S),$(D),libamas-utils.so)
	$(call inst,$(S),$(D),amas-utils.h)

ahs:
	$(call inst,$(S),$(D),ahs)

libasc:
	$(call inst,$(S),$(D),libasc.so)

asd:
	$(call inst,$(S),$(D),asd)

.PHONY: all clean strip $(pb-y) $(pb-m)
