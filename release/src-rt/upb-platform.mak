include upb.inc
include .config
-include $(TOPPREFIX)/.config
-include $(LINUXDIR)/.config
-include $(SRCBASE)/router/.config

ifeq ($(RTCONFIG_BCMARM),y)
export  BCMEX := _arm
ifeq ($(RTCONFIG_BCM7),y)
export  EX7 := _7
else ifeq ($(RTCONFIG_BCM_7114),y)
export  EX7 := _7114
else ifeq ($(RTCONFIG_BCM9),y)
export  EX7 := _9
else ifeq ($(HND_ROUTER_AX),y)
export  BCMEX :=
export  EX7 :=
export APPS_POSTFIX := bcmdrivers/broadcom/net/wl/impl$(BCM_WLIMPL)/main/components/apps
else ifeq ($(HND_ROUTER),y)
export  EX7 := _94908hnd
endif
endif

ifeq ($(RTCONFIG_RALINK),y)
ifeq ($(CONFIG_LINUX30),y)
export UFSDPATH := ralink_3.0.0
else
export UFSDPATH := ralink_2.6.2219
endif
else
ifeq ($(RTCONFIG_BCMARM),y)
ifeq ($(ARMCPUSMP),up)
export UFSDPATH := broadcom_arm_up
else
export UFSDPATH := broadcom_arm
endif
else
export UFSDPATH := broadcom
endif
endif

ifeq ($(RTCONFIG_HND_ROUTER_AX_675X),y)
ifeq (,$(wildcard ./chip_profile.tmp))
include ./chip_profile.mak
export CUR_CHIP_PROFILE_TMP ?= $(shell echo $(MAKECMDGOALS)_CHIP_PROFILE | tr a-z A-Z)
$(shell echo "CUR_CHIP_PROFILE=$($(CUR_CHIP_PROFILE_TMP))" > ./chip_profile.tmp)
$(shell echo "CUR_CHIP_PROFILE=$($(CUR_CHIP_PROFILE_TMP))" > router/chip_profile.tmp)
endif
include ./chip_profile.tmp
export HND_SRC := $(shell pwd | sed 's/\(.*src-rt-.*hnd.*\).*/\1/')
export BCM_WLIMPL
export PROFILE ?= 9$(CUR_CHIP_PROFILE)GW
include $(HND_SRC)/targets/$(PROFILE)/$(PROFILE)
endif

pb-$(CONFIG_LIBBCM)		+= libbcm
pb-$(CONFIG_LIBUPNP)		+= libupnp$(BCMEX)$(EX7)
ifeq ($(RTCONFIG_BCMARM),y)
pb-y				+= acsd$(BCMEX)$(EX7)
pb-y				+= eapd$(BCMEX)$(EX7)/linux
pb-y				+= nas$(BCMEX)$(EX7)/nas
pb-y				+= wps$(BCMEX)$(EX7)
pb-y				+= utils$(BCMEX)$(EX7)
pb-$(RTCONFIG_WLEXE)		+= wlexe
pb-y				+= emf$(BCMEX)$(EX7)/emfconf
pb-y				+= emf$(BCMEX)$(EX7)/igsconf
pb-y				+= emf$(BCMEX)$(EX7)
pb-y				+= wl$(BCMEX)$(EX7)
pb-y				+= et$(BCMEX)$(EX7)
pb-$(RTCONFIG_BCM_7114)		+= dhd_monitor
pb-$(RTCONFIG_HSPOT)		+= hspot_ap$(BCMEX)$(EX7)
pb-$(RTCONFIG_TOAD)		+= toad
pb-$(RTCONFIG_BCMBSD)		+= bsd
pb-$(RTCONFIG_BT_CONN)		+= btconfig
pb-$(RTCONFIG_BCMBSD)		+= appeventd
pb-$(RTCONFIG_BCMSSD)		+= ssd
pb-$(RTCONFIG_PARAGON_NTFS)	+= ufsd
pb-$(RTCONFIG_PARAGON_HFS)	+= ufsd
pb-$(RTCONFIG_EXT_RTL8365MB)	+= rtl_bin
pb-y				+= wlconf$(BCMEX)$(EX7)
pb-$(RTCONFIG_LACP)		+= lacp
pb-$(RTCONFIG_BCMASPMD)		+= aspmd
pb-$(RTCONFIG_BCMEVENTD)	+= eventd
pb-y				+= rc
pb-$(HND_ROUTER)		+= dhd_monitor
pb-$(HND_ROUTER)		+= hnd_extra
pb-$(HND_ROUTER)		+= hnd
pb-$(HND_ROUTER)		+= hnd_dhd
pb-$(HND_ROUTER)		+= hnd_emf
pb-$(HND_ROUTER)		+= hnd_igs
pb-$(HND_ROUTER)		+= hnd_wl
pb-$(HND_ROUTER)		+= wlcsm
pb-$(HND_ROUTER)		+= wlan
pb-$(HND_ROUTER)		+= bcm_flashutil
pb-$(HND_ROUTER)		+= bcm_flasher
pb-$(HND_ROUTER)		+= bcm_boardctl
pb-$(HND_ROUTER)		+= tmctl_lib
pb-$(HND_ROUTER)		+= tmctl
pb-$(HND_ROUTER)		+= ethswctl
pb-$(HND_ROUTER)		+= ethswctl_lib
pb-$(HND_ROUTER)		+= bcmtm
pb-$(HND_ROUTER)		+= bcmtm_lib
pb-$(HND_ROUTER)		+= rdpactl
pb-$(HND_ROUTER)		+= vlanctl
pb-$(HND_ROUTER)		+= vlanctl_lib
pb-$(HND_ROUTER)		+= seltctl
pb-$(HND_ROUTER)		+= bridgeutil
pb-$(HND_ROUTER)		+= pwrctl
pb-$(HND_ROUTER)		+= pwrctl_lib
pb-$(HND_ROUTER)		+= stlport
pb-$(HND_ROUTER)		+= bpmctl
pb-$(HND_ROUTER)		+= dnsspoof
pb-$(HND_ROUTER)		+= ethctl
pb-$(HND_ROUTER)		+= ethctl_lib
pb-$(HND_ROUTER)		+= fcctl
pb-$(HND_ROUTER)		+= fcctl_lib
pb-$(HND_ROUTER)		+= stress
pb-$(HND_ROUTER)		+= vpmstats
pb-$(HND_ROUTER)		+= bcm_boot_launcher
pb-$(HND_ROUTER)		+= swmdk
pb-$(HND_ROUTER)		+= mdk
pb-$(HND_ROUTER)		+= ivitcl
pb-$(HND_ROUTER)		+= bcmmcast
pb-$(HND_ROUTER)		+= bcmmcastctl
pb-$(HND_ROUTER)		+= mcpctl
pb-$(HND_ROUTER)		+= mcpd
pb-$(HND_ROUTER)		+= hostTools
pb-$(HND_ROUTER)		+= bdmf_shell
pb-$(HND_ROUTER)		+= psictl
pb-$(HND_ROUTER)		+= scratchpadctl
pb-$(HND_ROUTER)		+= wdtctl
pb-$(HND_ROUTER)		+= httpdshared
pb-$(HND_ROUTER)		+= bdmf_lib
pb-$(HND_ROUTER)		+= mdkshell
pb-$(HND_ROUTER)		+= bcm_bootstate
pb-$(HND_ROUTER_AX)		+= acsdv2
pb-$(HND_ROUTER_AX)		+= cevent_app
else
pb-$(RTCONFIG_BCMWL6)		+= acsd
pb-y				+= eapd/linux
pb-y				+= nas/nas
pb-y				+= wps
pb-y				+= utils
pb-$(RTCONFIG_EMF)		+= emf/emfconf
pb-$(RTCONFIG_EMF)		+= emf/igsconf
pb-$(RTCONFIG_EMF)		+= emf
pb-y				+= wlconf
pb-$(RTCONFIG_BCMWL6)		+= igmp
endif
pb-$(RTCONFIG_TMOBILE)		+= radpd
pb-$(RTCONFIG_SNMPD)		+= net-snmp-5.7.2
pb-$(RTCONFIG_SNMPD)		+= libnmp
pb-$(RTCONFIG_CLOUDSYNC)	+= inotify

ifeq ($(CONFIG_BCMWL5),y)
pb-$(RTCONFIG_WLCEVENTD)        += wlceventd
endif
ifeq ($(RTCONFIG_RALINK),y)
pb-$(RTCONFIG_WLCEVENTD)        += iwevent
endif
ifeq ($(RTCONFIG_QCA),y)
pb-$(RTCONFIG_WLCEVENTD)        += qca-wifi-assoc-eventd
endif

ifeq ($(or $(RTCONFIG_ALPINE),$(RTCONFIG_LANTIQ),$(RTCONFIG_BCMWL6),$(CONFIG_BCMWL5)),y)
pb-y                            += ctools
endif

ifeq ($(RTCONFIG_HND_ROUTER_AX_675X),y)
pb-y				+= rtecdc
endif

ifeq ($(RTCONFIG_LANTIQ),y)
pb-y                            += libfapi-0.1
pb-y                            += libhelper-1.4.0.2
pb-y                            += fapi_wlan_common-05.04.00.131
endif

all: $(pb-y) $(pb-m)
#all: acsd

############################################################################
# Generate short variable for destination directory.
# NOTE: Only one variable can be defined in one target.
############################################################################
$(pb-y) $(pb-m): S=$(TOPPREFIX)/$@
$(pb-y) $(pb-m): D=$(PBTOPDIR)/$@/prebuilt

############################################################################
# Define special S or D variable here.
# NOTE: Only one variable can be defined in one target.
############################################################################
btconfig: D=$(PBTOPDIR)/$@/prebuild
utils: D=$(PBTOPDIR)/$@/../../../$(SRCBASEDIR)/wl/exe/prebuilt
utils$(BCMEX)$(EX7): D=$(PBTOPDIR)/$@/../../../$(SRCBASEDIR)/wl/exe/prebuilt
wlexe: D=$(PBTOPDIR)/$@/../../../$(SRCBASEDIR)/wl/exe/prebuilt
emf: S=$(LINUXDIR)/drivers/net/
emf$(BCMEX)$(EX7): S=$(LINUXDIR)/drivers/net/
wl$(BCMEX)$(EX7): S=$(LINUXDIR)/drivers/net
hnd: D=$(PBTOPDIR)/hnd_extra/prebuilt
hnd_dhd: D=$(PBTOPDIR)/hnd_extra/prebuilt
hnd_emf: D=$(PBTOPDIR)/hnd_extra/prebuilt
hnd_igs: D=$(PBTOPDIR)/hnd_extra/prebuilt
hnd_wl: D=$(PBTOPDIR)/hnd_extra/prebuilt
et$(BCMEX)$(EX7): S=$(LINUXDIR)/drivers/net
rtl_bin: S=$(LINUXDIR)/drivers/char/rtl8365mb/
lacp: D=$(PBTOPDIR)/$@/linux
rc: D=$(PBTOPDIR)/$@/prebuild
inotify: D=$(PBTOPDIR)/$@/prebuild
wlan: D=$(PBTOPDIR)/$@

ifneq ($(wildcard $(shell pwd)/router-sysdep),)
pb-sysdep := $(shell ls $(TOP_PLATFORM))
$(pb-sysdep): S=$(TOP_PLATFORM)/$@
$(pb-sysdep): D=$(PBPREFIX)/$(PLATFORMSRC)/router-sysdep/$@/prebuilt
wlan: D=$(PBPREFIX)/$(PLATFORMSRC)/router-sysdep/$@
eapd/linux: S=$(TOP_PLATFORM)/$@
eapd/linux: D=$(PBPREFIX)/$(PLATFORMSRC)/router-sysdep/$@/prebuilt
emf/emfconf: S=$(TOP_PLATFORM)/$@
emf/emfconf: D=$(PBPREFIX)/$(PLATFORMSRC)/router-sysdep/$@/prebuilt
emf/igsconf: S=$(TOP_PLATFORM)/$@
emf/igsconf: D=$(PBPREFIX)/$(PLATFORMSRC)/router-sysdep/$@/prebuilt
nas/nas: S=$(TOP_PLATFORM)/$@
nas/nas: D=$(PBPREFIX)/$(PLATFORMSRC)/router-sysdep/$@/prebuilt
endif

hnd_extra: S=$(HND_SRC)
mdk: S=$(TOP_PLATFORM)/mdk/examples/linux-user
acsdv2: S=$(SRCBASE)/$(APPS_POSTFIX)/$@
acsdv2: D=$(PBPREFIX)/$(PLATFORMSRC)/$(APPS_POSTFIX)/$@/prebuilt
cevent_app:  S=$(SRCBASE)/$(APPS_POSTFIX)/$@
cevent_app:  D=$(PBPREFIX)/$(PLATFORMSRC)/$(APPS_POSTFIX)/$@/prebuilt
############################################################################
# Copy binary
############################################################################
acsd:
	$(call inst,$(S),$(D),acsd)
	$(call inst,$(S),$(D),acsd_cli)

acsd$(BCMEX)$(EX7):
	$(call inst,$(S),$(D),acsd)
	$(call inst,$(S),$(D),acs_cli)

acsdv2:
	$(call inst,$(S),$(D),acsd2)
	$(call inst,$(S),$(D),acs_cli2)

cevent_app:
	$(call inst,$(S),$(D),ceventd)
	$(call inst,$(S),$(D),ceventc)

appeventd:
	$(call inst,$(S),$(D),appeventd)

aspmd:
	$(call inst,$(S),$(D),aspmd)

#bcm_util:
#	$(call inst,$(S),$(D),libbcm_crc.so)
#	$(call inst,$(S),$(D),bcm_crc.h)
	
bdmf_lib:
	$(call inst,$(S),$(D),libbdmf.so)

bdmf_shell:
	$(call inst,$(S),$(D),bdmf_shell)

bsd:
	$(call inst,$(S),$(D),bsd)

bcm_flashutil:
	$(call inst,$(S),$(D),libbcm_flashutil.so)
	$(call inst,$(S),$(D)/../,bcm_flashutil.h)

bcm_flasher:
	$(call inst,$(S),$(D),bcm_flasher)

bcm_boardctl:
	$(call inst,$(S),$(D),libbcm_boardctl.so)

bcm_boot_launcher:
	$(call inst,$(S),$(D),bcm_boot_launcher)

bcm_bootstate:
	$(call inst,$(S),$(D),bcm_bootstate)

bcmtm:
	$(call inst,$(S),$(D),bcmtmctl)

bcmtm_lib:
	$(call inst,$(S),$(D),libbcmtm.so)

bridgeutil:
	$(call inst,$(S),$(D),libbridgeutil.so)

bpmctl:
	$(call inst,$(S),$(D),bpmctl)

bcmmcast:
	$(call inst,$(S),$(D),libbcmmcast.so)

bcmmcastctl:
	$(call inst,$(S),$(D),bcmmcastctl)

btconfig:
	-$(call inst,$(S),$(D),btconfig)

ctools:
	-install -D $(SRCBASE)/ctools/trx_asus ${PBTOPDIR}/../../$(SRCBASEDIR)/ctools/prebuild/trx_asus
	-install -D $(SRCBASE)/ctools/Makefile ${PBTOPDIR}/../../$(SRCBASEDIR)/ctools/Makefile

rtecdc:
	-mkdir -p ${PBTOPDIR}/../../$(SRCBASEDIR)/bcmdrivers/broadcom/net/wl/impl$(BCM_WLIMPL)/sys/src/dongle/sysdeps/default
	-install -D $(SRCBASE)/bcmdrivers/broadcom/net/wl/impl$(BCM_WLIMPL)/sys/src/dongle/bin/43684b0/rtecdc.bin ${PBTOPDIR}/../../$(SRCBASEDIR)/bcmdrivers/broadcom/net/wl/impl$(BCM_WLIMPL)/sys/src/dongle/sysdeps/default/43684b0/rtecdc.bin

dnsspoof:
	$(call inst,$(S),$(D),dnsspoof)

dhd_monitor:
	$(call inst,$(S),$(D),dhd_monitor)
	$(call inst,$(S),$(D),debug_monitor)

dhrystone:
	$(call inst,$(S),$(D),dry)

et$(BCMEX)$(EX7):
	$(call inst,$(S)/et,$(D),et.o)

eapd/linux:
	$(call inst,$(S),$(D),eapd)

eapd$(BCMEX)$(EX7)/linux:
	$(call inst,$(S),$(D),eapd)

emf/emfconf:
	$(call inst,$(S),$(D),emf)

emf$(BCMEX)$(EX7)/emfconf:
	$(call inst,$(S),$(D),emf)

emf/igsconf:
	echo ${S}
	$(call inst,$(S),$(D),igs)

emf$(BCMEX)$(EX7)/igsconf:
	echo ${S}
	$(call inst,$(S),$(D),igs)

emf:
	$(call inst,$(S)/emf,$(D)/..,emf.o)
	$(call inst,$(S)/igs,$(D)/..,igs.o)

emf$(BCMEX)$(EX7):
	$(call inst,$(S)/emf,$(D)/..,emf.o)
	$(call inst,$(S)/igs,$(D)/..,igs.o)

eventd:
	$(call inst,$(S),$(D),eventd)

ethswctl:
	$(call inst,$(S),$(D),ethswctl)

ethswctl_lib:
	$(call inst,$(S),$(D),libethswctl.so)

ethctl:
	$(call inst,$(S),$(D),ethctl)

ethctl_lib:
	$(call inst,$(S),$(D),libethctl.so)

fapi_wlan_common-1.0.0.1:
	$(call inst,$(S),$(D),libfapiwlancommon.so)

fcctl:
	$(call inst,$(S),$(D),fcctl)

fcctl_lib:
	$(call inst,$(S),$(D),libfcctl.so)

httpdshared:
	$(call inst,$(S),$(D),libhttpdshared.so)

hspot_ap$(BCMEX)$(EX7):
	$(call inst,$(S),$(D),hspotap)
	$(call inst,$(S),$(D)/..,hspotap.c)


hostTools:
	-install -D $(SRCBASE)/hostTools/addvtoken ${PBTOPDIR}/../../$(SRCBASEDIR)/hostTools/prebuilt/addvtoken

hnd_extra:
	$(call inst,$(S)/bcmdrivers/opensource/net/enet/impl5,$(D),bcm_enet.o)
	$(call inst,$(S)/bcmdrivers/opensource/net/enet/impl7,$(D),bcm_enet.o)
	$(call inst,$(S)/bcmdrivers/opensource/net/wfd/impl1/,$(D),wfd.o)
	$(call inst,$(S)/bcmdrivers/opensource/char/pdc/impl1,$(D),bcmpdc.o)
	$(call inst,$(S)/bcmdrivers/opensource/char/spudd/impl4,$(D),bcmspu.o)
	$(call inst,$(S)/bcmdrivers/opensource/char/map/impl1,$(D),ivi_map.h)
	$(call inst,$(S)/bcmdrivers/opensource/char/map/impl1,$(D),ivi_config.h)
	$(call inst,$(S)/bcmdrivers/opensource/char/plat-bcm/impl1,$(D),bcm_arm64_setup.o)
	$(call inst,$(S)/bcmdrivers/opensource/char/plat-bcm/impl1,$(D),bcm_arm_cpuidle.o)
	$(call inst,$(S)/bcmdrivers/opensource/char/plat-bcm/impl1,$(D),bcm_arm_irq.o)
	$(call inst,$(S)/bcmdrivers/opensource/char/plat-bcm/impl1,$(D),bcm_dt.o)
	$(call inst,$(S)/bcmdrivers/opensource/char/plat-bcm/impl1,$(D),bcm_extirq.o)
	$(call inst,$(S)/bcmdrivers/opensource/char/plat-bcm/impl1,$(D),bcm_i2c.o)
	$(call inst,$(S)/bcmdrivers/opensource/char/plat-bcm/impl1,$(D),bcm_legacy_io_map.o)
	$(call inst,$(S)/bcmdrivers/opensource/char/plat-bcm/impl1,$(D),bcm_thermal.o)
	$(call inst,$(S)/bcmdrivers/opensource/char/plat-bcm/impl1,$(D),blxargs.o)
	$(call inst,$(S)/bcmdrivers/opensource/char/plat-bcm/impl1,$(D),setup.o)
	$(call inst,$(S)/bcmdrivers/opensource/char/plat-bcm/impl1,$(D),bcm_usb.o)
	$(call inst,$(S)/bcmdrivers/opensource/char/fpm/impl1,$(D),rdp_fpm.o)
	$(call inst,$(S)/bcmdrivers/opensource/char/rdpa_drv/impl1,$(D),rdpa_cmd.o)
	$(call inst,$(S)/bcmdrivers/opensource/char/rdpa_gpl_ext/impl1,$(D),rdpa_gpl_ext.o)
	$(call inst,$(S)/bcmdrivers/opensource/char/rdpa_mw/impl1,$(D),rdpa_mw.o)
	$(call inst,$(S)/bcmdrivers/broadcom/char/wlcsm_ext/impl1,$(D),wlcsm.o)
	$(call inst,$(S)/bcmdrivers/broadcom/char/pktrunner/impl2,$(D),pktrunner.o)
	$(call inst,$(S)/bcmdrivers/broadcom/char/vlan/impl1,$(D),bcmvlan.o)
	$(call inst,$(S)/bcmdrivers/broadcom/char/chipinfo/impl1,$(D),chipinfo.o)
	$(call inst,$(S)/bcmdrivers/broadcom/char/cmdlist/impl1,$(D),cmdlist.o)
	$(call inst,$(S)/bcmdrivers/broadcom/char/tms/impl1,$(D),nciTMSkmod.o)
	$(call inst,$(S)/bcmdrivers/broadcom/char/pktflow/impl1,$(D),pktflow.o)
	$(call inst,$(S)/bcmdrivers/broadcom/char/pwrmngt/impl1,$(D),pwrmngtd.o)
	$(call inst,$(S)/bcmdrivers/broadcom/net/wl/impl51/dhd/src/shared/bcmwifi/include,$(D),bcmwifi_rates.h)
	$(call inst,$(S)/bcmdrivers/broadcom/net/wl/impl51/main/src/wl/sys,$(D),wlc_types.h)
	$(call inst,$(S)/rdp/projects/WL4908/target/bdmf,$(D),bdmf.o)
	$(call inst,$(S)/rdp/projects/WL4908/target/rdpa,$(D),rdpa.o)
	$(call inst,$(S)/rdp/projects/WL4908/target/rdpa_gpl,$(D),rdpa_gpl.o)
	$(call inst,$(S)/router/wlexe,$(D),wl)

hnd_dhd:
	$(call inst,$(S),$(D),dhd.o)

hnd_wl:
	$(call inst,$(S),$(D),wl.o)

hnd_emf:
	$(call inst,$(S),$(D),emf.o)

hnd_igs:
	$(call inst,$(S),$(D),igs.o)

hnd:
	$(call inst,$(S),$(D),hnd.o)

igmp:
	$(call inst,$(S),$(D),igmp)

inotify:
	$(call inst,$(S),$(D),inotify)

iwevent:
	$(call inst,$(S),$(D),wlceventd)

ivitcl:
	$(call inst,$(S),$(D),ivitcl)

libbcm:
	$(call inst,$(S),$(D),libbcm.so)

libupnp$(BCMEX)$(EX7):
	$(call inst,$(S),$(D),libupnp.so)
	$(call inst,$(S),$(D)/..,Makefile)

lacp:
	$(call inst,$(S),$(D),lacp.o)

libfapi-0.1:
	$(call inst,$(S),$(D),libfapi.so)

libhelper-1.4.0.2:
	$(call inst,$(S),$(D),libhelper.so)

libnmp:
	$(call inst,$(S),$(D),libnmp.so)

mdk:
	$(call inst,$(S)/bmd,$(D),libbmdapi.so)
	$(call inst,$(S)/bmd,$(D),libbmdpkgsrc.so)
	$(call inst,$(S)/bmd,$(D),libbmdshared.so)
	$(call inst,$(S)/bmd,$(D),libbmdshell.so)
	$(call inst,$(S)/cdk,$(D),libcdkdsym.so)
	$(call inst,$(S)/cdk,$(D),libcdklibc.so)
	$(call inst,$(S)/cdk,$(D),libcdkmain.so)
	$(call inst,$(S)/cdk,$(D),libcdkpkgsrc.so)
	$(call inst,$(S)/cdk,$(D),libcdkshared.so)
	$(call inst,$(S)/cdk,$(D),libcdkshell.so)
	$(call inst,$(S)/cdk,$(D),libcdksym.so)
	$(call inst,$(S)/phy,$(D),libphygeneric.so)
	$(call inst,$(S)/phy,$(D),libphypkgsrc.so)
	$(call inst,$(S)/phy,$(D),libphysym.so)
	$(call inst,$(S)/phy,$(D),libphyutil.so)

mdkshell:
	$(call inst,$(S),$(D),mdkshell)

mcpctl:
	$(call inst,$(S),$(D),mcpctl)

mcpd:
	$(call inst,$(S),$(D),mcpd)

nas/nas:
	$(call inst,$(S),$(D),nas)

nas$(BCMEX)$(EX7)/nas:
	$(call inst,$(S),$(D),nas)

net-snmp-5.7.2:
	$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME),$(D)/../mibs,$(BUILD_NAME)-MIB.txt)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,administration.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,administration.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,guestNetwork.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,guestNetwork.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,lan.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,lan.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,quickInternetSetup.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,quickInternetSetup.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,trafficManager.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,trafficManager.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,wan.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,wan.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,firewall.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,firewall.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,ipv6.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,ipv6.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,parentalControl.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,parentalControl.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,systemLog.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,systemLog.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,vpn.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,vpn.h)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,wireless.c)
	-$(call inst,$(S)/asus_mibs/sysdeps/$(BUILD_NAME)/asus-mib,$(D)/../agent/asus-mib,wireless.h)

psictl:
	$(call inst,$(S),$(D),psictl)

pwrctl:
	$(call inst,$(S),$(D),pwrctl)

pwrctl_lib:
	$(call inst,$(S),$(D),libpwrctl.so)

qca-wifi-assoc-eventd:
	$(call inst,$(S),$(D),wlceventd)

rc:
	-$(call inst,$(S),$(D),ate-broadcom.o)
	-$(call inst,$(S),$(D),init-broadcom.o)
	-$(call inst,$(S),$(D),ate.o)
ifeq ($(BUILD_NAME),$(filter $(BUILD_NAME),RT-AC68U RT-AC3200 RT-AX58U TUF-AX3000 RT-AX82U))
	-$(call inst,$(S),$(D),cfe.o)
endif
	-$(call inst,$(S),$(D),ctl.o)
	-$(call inst,$(S),$(D),dsl_fb.o)
	-$(call inst,$(S),$(D),dsl_diag.o)
	-$(call inst,$(S),$(D),pwdec.o)
	-$(call inst,$(S),$(D),ate-ralink.o)
	-$(call inst,$(S),$(D),ate-qca.o)
	-$(call inst,$(S),$(D),tcode_rc.o)
	-$(call inst,$(S),$(D),tcode_brcm.o)
	-$(call inst,$(S),$(D),agg_brcm.o)
	-$(call inst,$(S),$(D),dualwan.o)
	-$(call inst,$(S),$(D),bwdpi_check.o)
	-$(call inst,$(S),$(D),bwdpi_db_10.o)
	-$(call inst,$(S),$(D),bwdpi_wred_alive.o)
	-$(call inst,$(S),$(D),nt_mail.o)
	-$(call inst,$(S),$(D),obd.o)
	-$(call inst,$(S),$(D),obd_eth.o)
	-$(call inst,$(S),$(D),obd-broadcom.o)
	-$(call inst,$(S),$(D),obd_monitor.o)
	-$(call inst,$(S),$(D),adtbw-broadcom.o)
	-$(call inst,$(S),$(D),amas_adtbw.o)
	-$(call inst,$(S),$(D),amas-adtbw-broadcom.o)
ifeq ($(RTCONFIG_BHCOST_OPT),y)
	-$(call inst,$(S),$(D),amas-ssd-broadcom.o)
endif
	-$(call inst,$(S),$(D),roamast.o)
	-$(call inst,$(S),$(D),psta_monitor.o)
	-$(call inst,$(S),$(D),broadcom.o)
	-$(call inst,$(S),$(D),roamast-broadcom.o)
	-$(call inst,$(S),$(D),roamast-lantiq.o)
ifeq ($(RTCONFIG_CONNDIAG),y)
	-$(call inst,$(S),$(D),conn_diag-broadcom.o)
endif
ifeq ($(RTCONFIG_BRCM_HOSTAPD),y)
	-$(call inst,$(S),$(D),wps_pbcd.o)
	-$(call inst,$(S),$(D),hostapd_config.o)
endif
ifeq ($(BUILD_NAME),$(filter $(BUILD_NAME),RT-AX82U))
	-$(call inst,$(S),$(D),ledg.o)
endif

rdpactl:
	$(call inst,$(S),$(D),librdpactl.so)

rtl_bin:
	$(call inst,$(S),$(D),rtl8365mb.o)

radpd:
	$(call inst,$(S),$(D),radpd)

wdtctl:
	$(call inst,$(S),$(D),wdtctl)

wlexe:
	$(call inst,$(S),$(D),wl)

wl$(BCMEX)$(EX7):
	-$(call inst,$(S)/wl,$(D),wl_apsta.o)
	-$(call inst,$(S)/wl,$(D),wl.o)
	-mv $(D)/wl.o $(D)/wl_apsta.o
	-$(call inst,$(S)/dhd,$(D),dhd.o)
	-$(call inst,$(S)/dhd24,$(D),dhd24.o)
ssd:
	$(call inst,$(S),$(D),ssd)

seltctl:
	$(call inst,$(S),$(D),seltctl)

stlport:
	$(call inst,$(S),$(D),libstlport.so.5.2)

stress:
	$(call inst,$(S),$(D),stress)

swmdk:
	$(call inst,$(S),$(D),swmdk)

scratchpadctl:
	$(call inst,$(S),$(D),scratchpadctl)

toad:
	$(call inst,$(S),$(D),toad)
	$(call inst,$(S),$(D),toast)

tmctl_lib:
	$(call inst,$(S),$(D),libtmctl.so)

tmctl:
	$(call inst,$(S),$(D),tmctl)

ufsd:
	@echo "UFSDPATH=${UFSDPATH}"
	$(call inst,$(S)/$(UFSDPATH),$(D)/../$(UFSDPATH),ufsd.ko)
	$(call inst,$(S)/$(UFSDPATH),$(D)/../$(UFSDPATH),jnl.ko)
ifeq ($(RTCONFIG_PARAGON_NTFS),y)
	$(call inst,$(S)/$(UFSDPATH),$(D)/../$(UFSDPATH),chkntfs)
	$(call inst,$(S)/$(UFSDPATH),$(D)/../$(UFSDPATH),mkntfs)
endif
ifeq ($(RTCONFIG_PARAGON_HFS),y)
	$(call inst,$(S)/$(UFSDPATH),$(D)/../$(UFSDPATH),chkhfs)
	$(call inst,$(S)/$(UFSDPATH),$(D)/../$(UFSDPATH),mkhfs)
endif

utils:
	$(call inst,$(S),$(D),wl)

utils$(BCMEX)$(EX7):
	-$(call inst,$(S),$(D),et)
	$(call inst,$(S),$(D),wl)

vpmstats:
	$(call inst,$(S),$(D),vpmstats)

vlanctl:
	$(call inst,$(S),$(D),vlanctl)

vlanctl_lib:
	$(call inst,$(S),$(D),libvlanctl.so)

wlceventd:
	$(call inst,$(S),$(D),wlceventd)

wlcsm:
	$(call inst,$(S),$(D),libwlcsm.so)

wlan:
	$(call inst,$(S)/nvram,$(D)/nvram/prebuilt,libnvram.so)
	$(call inst,$(S)/nvram,$(D)/nvram/prebuilt,nvram)
	$(call inst,$(S)/util,$(D)/util/prebuilt,nvramUpdate)

wps:
	echo ${S} ${D}
	$(call inst,$(S),$(D),wps_monitor)

wlconf:
	$(call inst,$(S),$(D),wlconf)

wlconf$(BCMEX)$(EX7):
	$(call inst,$(S),$(D),wlconf)

wps$(BCMEX)$(EX7):
	echo ${S} ${D}
	$(call inst,$(S),$(D),wps_monitor)

fapi_wlan_common-05.04.00.131:
	$(call inst,$(S),$(D),libfapiwlancommon.so)

.PHONY: all $(pb-y) $(pb-m)

