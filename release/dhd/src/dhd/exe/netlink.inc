#
# netlink.inc for dhd/exe
#
# Copyright (c) 2013, Broadcom Corp.
# $Id: netlink.inc 454499 2014-02-10 23:59:37Z $

ifdef NL80211

ifneq ($(findstring android,$(TARGETENV)),)

include $(dir $(lastword $(MAKEFILE_LIST)))/../../makefiles/WLAN_Common.mk

ifeq ($(NLHEADERS_PATH),)
$(error libnl headers path must be specified)
endif
ifeq ($(NLSTATICLIB_PATH),)
$(error libnl static library path must be specified)
endif

CFLAGS += -I$(NLHEADERS_PATH)
LIBS += -Wl,--whole-archive -Wl,--no-whole-archive $(NLSTATICLIB_PATH)

LIBNLVER = $(shell grep "LIBNL_VERSION" $(NLHEADERS_PATH)/netlink/version.h | cut -d " " -f 3 | sed "s/\"//g")

ifeq ($(call wlan_version_ge,$(LIBNLVER),3.0),TRUE)
CFLAGS += -DCONFIG_LIBNL30
else ifeq ($(call wlan_version_ge,$(LIBNLVER),2.0),TRUE)
CFLAGS += -DCONFIG_LIBNL20
endif

else #ifneq ($(findstring android,$(TARGETENV)),)

PKG_CONFIG ?= pkg-config

NL1FOUND := $(shell $(PKG_CONFIG) --atleast-version=1 libnl-1 && echo Y)
NL2FOUND := $(shell $(PKG_CONFIG) --atleast-version=2 libnl-2.0 && echo Y)
NL3FOUND := $(shell $(PKG_CONFIG) --atleast-version=3 libnl-3.0 && echo Y)
NL31FOUND := $(shell $(PKG_CONFIG) --exact-version=3.1 libnl-3.1 && echo Y)
NL3xFOUND := $(shell $(PKG_CONFIG) --atleast-version=3.2 libnl-3.0 && echo Y)

$(sh echo "what to do")

ifeq ($(NL1FOUND),Y)
NLLIBNAME = libnl-1
endif

ifeq ($(NL2FOUND),Y)
CFLAGS += -DCONFIG_LIBNL20
LIBS += -lnl-genl
NLLIBNAME = libnl-2.0
endif

ifeq ($(NL3xFOUND),Y)
# libnl 3.2 might be found as 3.2 and 3.0
NL3FOUND = N
CFLAGS += -DCONFIG_LIBNL30
LIBS += -lnl-genl-3
NLLIBNAME = libnl-3.0
endif

ifeq ($(NL3FOUND),Y)
CFLAGS += -DCONFIG_LIBNL30
LIBS += -lnl-genl
NLLIBNAME = libnl-3.0
endif

# nl-3.1 has a broken libnl-gnl-3.1.pc file
# as show by pkg-config --debug --libs --cflags --exact-version=3.1 libnl-genl-3.1;echo $?
ifeq ($(NL31FOUND),Y)
CFLAGS += -DCONFIG_LIBNL30
LIBS += -lnl-genl
NLLIBNAME = libnl-3.1
endif

ifeq ($(NLLIBNAME),)
$(error Cannot find development files for any supported version of libnl)
endif

LIBS += $(shell $(PKG_CONFIG) --libs $(NLLIBNAME))
CFLAGS += $(shell $(PKG_CONFIG) --cflags $(NLLIBNAME))

endif #ifneq ($(findstring android,$(TARGETENV)),)

ifeq ($(BUILD_TAG),TRUNK)
DHD_OBJS += $(TARGETARCH)/dhdu_nl80211.o
else
DHD_OBJS += dhdu_nl80211.o
endif

endif # NL80211
