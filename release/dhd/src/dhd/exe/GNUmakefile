#
# GNUmakefile for dhd/exe
#
# Copyright (c) 2000, Broadcom Corp.
# $Id: GNUmakefile,v 1.27 2010-09-13 14:20:34 $

WLAN_ComponentsInUse := bcmwifi
ifeq ($(CONFIG_DHDAP),y)
ifeq ($(DHDAP_USE_SEPARATE_CHECKOUTS),1)
    export WLAN_TreeBaseA=$(abspath ../../..)
endif
endif
include ../../makefiles/WLAN_Common.mk
SRCBASE := $(WLAN_SrcBaseR)

UNAME = $(shell uname)

#-----------------------------------------------------------------
# Windows build
# 1) windows, don't include Makerules due to all: conflict

ifeq ($(findstring CYGWIN,$(UNAME)),CYGWIN)

include ../../GNUmakefile.inc

else # UNAME

# 2) not windows, need to include first to pick up TARGETENV dependent vars
include $(SRCBASE)/Makerules

# use nl80211 for driver access (ioctl replacement)
ifdef NL80211
CFLAGS += -DNL80211
endif

#ifdef BCMWPA2
CFLAGS += -DBCMWPA2
#endif
# Always include pktgen commands in the application
CFLAGS += -DSDTEST
#ifndef OEM_ANDROID
CFLAGS += -DBCMSPI
#endif

#ifdef WLCNT
CFLAGS += -DWLCNT
#endif


CFLAGS += -DWLBTAMP
CFLAGS += -DWLPFN -DLINUX -g -I$(SRCBASE)/wl/exe

vpath %.c $(SRCBASE)/shared $(SRCBASE)/wl/exe $(WLAN_StdSrcDirsR) $(WLAN_ComponentSrcDirsR)

#-----------------------------------------------------------------
# Linux build
#
# This should be one of values recognized in src/Makerules

# Change to the new dhd driver trunk build structure as discussed 
# by the build team 
ifeq ($(BUILD_TAG),TRUNK)

# $(TARGETARCH) is set based on TARGETENV in src/Makerules.* files
DHD_OBJS := $(TARGETARCH)/dhdu.o $(TARGETARCH)/dhdu_linux.o $(TARGETARCH)/bcmutils.o $(TARGETARCH)/bcmwifi_channels.o $(TARGETARCH)/wlu_client_shared.o $(TARGETARCH)/wlu_pipe_linux.o $(TARGETARCH)/wlu_pipe.o $(TARGETARCH)/miniopt.o $(TARGETARCH)/bcm_app_utils.o $(TARGETARCH)/ucode_download.o $(TARGETARCH)/wlu_common.o
DHD_EXE  := $(TARGETARCH)/dhd

include netlink.inc

all: $(DHD_EXE)

clean:
	 rm -fr $(DHD_EXE) $(DHD_OBJS) $(TARGETARCH)

$(DHD_EXE): $(DHD_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(TARGETARCH)/%.o: %.c
	@mkdir -p $(TARGETARCH)
	$(CC) -c $(CFLAGS) -DRWL_SOCKET -DRWL_WIFI -DRWL_DONGLE -o $@ $^

else # BUILD_TAG

# Preserve the old build structure for other components that still haven't not
# changed to the new build structure, once everyone moved to the new structure
# , we can remove the whole else BUILD_TAG code

ifneq ($(findstring $(TARGETENV), "linux linuxmips linuxmips_be linuxarm linuxarm_le android android_ndk_r6b"),)
# $(TARGETARCH) is set based on TARGETENV in src/Makerules.* files
DHD_OBJS := dhdu.o dhdu_linux.o bcmutils.o bcmwifi_channels.o wlu_client_shared.o wlu_pipe_linux.o wlu_pipe.o miniopt.o bcm_app_utils.o ucode_download.o wlu_common.o
DHD_EXE  := dhd
DHD_OBJS := $(DHD_OBJS:%.o=%.o)

include netlink.inc

ifeq ($(findstring $(TARGETARCH), "x86 x86_mmx"),)
  DHD_EXE  := $(DHD_EXE)$(TARGETARCH)
  DHD_OBJS := $(DHD_OBJS:%.o=$(TARGETARCH)/%.o)
endif

all: $(DHD_EXE)

clean:
	 rm -fr $(DHD_EXE) $(DHD_OBJS) $(TARGETARCH)

$(DHD_EXE): $(DHD_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# net_priv target is for building the dhd with NET_ADMIN privilege (i.e. to avoid running as root)
# note: The 'setcap' command is availble from the libcap2-bin package
# note: linux kernel < 2.6.33 doesn't support Security File Capabilities by default
net_priv: $(DHD_EXE)
	sudo setcap CAP_NET_ADMIN=eip $^

%.o: %.c
	$(CC) -c $(CFLAGS) -DRWL_SOCKET -DRWL_WIFI -DRWL_DONGLE -o $@ $^


ifeq ($(findstring $(TARGETARCH), "x86 x86_mmx"),)
$(TARGETARCH)/%.o: %.c
	@mkdir -p $(TARGETARCH)
	$(CC) -c $(CFLAGS) -DRWL_SOCKET -DRWL_WIFI -DRWL_DONGLE -o $@ $^
endif

endif # TARGETENV linux

endif # BUILD_TAG

.PHONY: all clean net_priv

endif # UNAME
