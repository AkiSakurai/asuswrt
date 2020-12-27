#
# In most cases, you only need to modify this first section.
#

EXE  = wlmngr wlevt wlconf
OBJS = wlmngr.o wlapi.o wlmdm.o wlsyscall.o wldsltr.o wlutil.o wlmngr_enum_ifnames.o nvram_mdm.o
ifeq ($(EXT_WLCONF),y)
OBJS += wl.o wl_linux.o shutils.o wlconf.o defaults.o bcmwifi_channels.o nvparse.o
endif
LIBS = -lcms_msg $(CMS_COMMON_LIBS) -ldl $(CMS_CORE_LIBS)
LIBS += -lwlmngr -lnvram -lwlctl

all dynamic: libwlmngr $(EXE)
install: all
	install -m 755 $(EXE) $(INSTALL_DIR)/bin

clean: generic_clean
	rm -f $(INSTALL_DIR)/bin/$(EXE)
	rm -f libwlmngr.*

#
# Set our CommEngine directory (by splitting the pwd into two words
# at /userspace and taking the first word only).
# Then include the common defines under CommEngine.
#
CURR_DIR := $(shell pwd)
BUILD_DIR:=$(HND_SRC)

include $(BUILD_DIR)/make.common
ifeq ($(BRCM_WAPI),y)
OBJS += wlmngr_wapi_cert_mdm.o wlmngr_wapi_cert_mgmt.o
endif

CFLAGS += $(SSP_TYP_COMPILER_OPTS)
LIBS += $(SSP_TYP_LIBS)

IMPL=$(shell readlink $(BUILD_DIR)/bcmdrivers/broadcom/net/wl/bcm9$(BRCM_CHIP) | sed 's/[^0-9]//g')
ifeq ($(IMPL),)
# the symbolic link of bcm9$(BRCM_CHIP) has been removed/cleaned, set IMPL to an integer to avoid warning message
IMPL=0
endif
WL_SRC_BASE=$(shell if [ "$(IMPL)" -ge 22 ]; then echo /main/src; fi ;)

WIRELESS_DRIVER_PATH=$(BRCMDRIVERS_DIR)/broadcom/net/wl/bcm9$(BRCM_CHIP)$(WL_SRC_BASE)
INC_WIRELESS_DRIVER_PATH=$(WIRELESS_DRIVER_PATH)/include
COMM_WIRELESS_DRIVER_PATH=$(WIRELESS_DRIVER_PATH)/common/include
#
# Private apps and libs are allowed to include header files from the
# private and public directories.
#
# WARNING: Do not modify this section unless you understand the
# license implications of what you are doing.
#
ALLOWED_INCLUDE_PATHS := -I.\
                         -I$(BUILD_DIR)/userspace/public/include  \
                         -I$(BUILD_DIR)/userspace/public/include/$(OALDIR) \
                         -I$(BUILD_DIR)/userspace/private/include  \
                         -I$(BUILD_DIR)/userspace/private/include/$(OALDIR) \
                         -I$(BUILD_DIR)/userspace/private/libs/cms_core \
                         -I$(BUILD_DIR)/userspace/private/libs/cms_core/linux 

# obviously, this app needs to access kernel headers
ALLOWED_INCLUDE_PATHS += -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD)  \
                         -I$(INC_BRCMDRIVER_PRIV_PATH)/$(BRCM_BOARD) \
                         -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) \
                         -I$(BUILD_DIR)/shared/broadcom/include/$(BRCM_BOARD) \
                         -I$(BUILD_DIR)/shared/opensource/include/$(BRCM_BOARD) \
                         -I$(INC_WIRELESS_DRIVER_PATH) \
			 -I$(COMM_WIRELESS_DRIVER_PATH) \
                         -I$(WIRELESS_DRIVER_PATH)/bcmcrypto \
                         -I$(WIRELESS_DRIVER_PATH)/shared \
                         -I$(WIRELESS_DRIVER_PATH)/router/shared \
                         -I$(WIRELESS_DRIVER_PATH)/shared/bcmwifi/include

ifeq ($(DSLCPE_WLCSM_EXT),1)
LIBS += -lwlcsm
CFLAGS += -DDSLCPE_WLCSM_EXT
ALLOWED_INCLUDE_PATHS += -I$(BUILD_DIR)/userspace/private/libs/wlcsm/include
endif

ifneq ($(strip $(WL_SRC_BASE)),)
CFLAGS+=-DWL_IMPL_PLUS
endif

ifeq ($(DSLCPE_CONFIG_MIPS),1)
CFLAGS+=-DCPU_MIPS
endif

CFLAGS += -DDSLCPE_DONGLEHOST_MFG

# Private apps and libs are allowed to link with libraries from the
# private and public directories.
#
# WARNING: Do not modify this section unless you understand the
# license implications of what you are doing.
#
ALLOWED_LIB_DIRS := /lib:/lib/public:/lib/private
#
# make endian adjustment
CFLAGS += -DDSLCPE_ENDIAN

ifeq (exists, $(shell [ -d $(WIRELESS_DRIVER_PATH)/router/eapd ]&& echo exists))
    CFLAGS += -DBUILD_EAPD -DDSLCPE -I$(WIRELESS_DRIVER_PATH)/router/eapd
endif

ifneq ($(strip $(DSLCPE_WPS_SEC_CLONE)),)
CFLAGS += -DDSLCPE_WPS_SEC_CLONE
endif

CFLAGS +=-DWIRELESS -DSUPPORT_MIMO -DDSLCPE_SHLIB -DBCMWPA2
CFLAGS += -DWMF
CFLAGS += -DDUCATI
ifneq ($(strip $(BUILD_WL_IDLE_PWRSAVE)),)
   CFLAGS += -DIDLE_PWRSAVE
endif

ifeq ($(BUILD_WLHSPOT),y)
CFLAGS += -DHSPOT_SUPPORT
endif

ifeq ($(EXT_ACS),y)
CFLAGS += -DEXT_ACS
endif

ifeq ($(EXT_WPS_VER2),y)
CFLAGS += -DWPS_V2
endif

ifneq ($(strip $(BUILD_WSC)), )
CFLAGS += -DSUPPORT_WSC
ifeq ($(strip $(BUILD_WSC_CONFIG_UPNP_DISABLE)),1)
CFLAGS += -DWSC_CONFIG_UPNP_DISABLE
endif
endif

ifeq ($(EXT_WL11AC),y)
CFLAGS += -DWL11AC
endif

ifeq ($(EXT_WLCONF),y)
CFLAGS += -DWLCONF
endif

ifneq ($(strip $(BUILD_IEEE1905)),)
CFLAGS += -DDSLCPE_1905
ALLOWED_INCLUDE_PATHS += -I$(BRCMDRIVERS_DIR)/broadcom/net/wl/bcm9$(BRCM_CHIP)$(WL_SRC_BASE)/wps/brcm_apps/include
endif

ifneq ($(strip $(MFP)),)
CFLAGS += -DMFP
endif

# treat all warnings as errors
#CUSTOM_CFLAGS += -Wno-error=unused-function -Wno-error=unused-but-set-variable -Werror -Wfatal-errors  # NEW_FORBID_WARNINGS

ifeq ($(EXT_WLCONF),y)
vpath %.c $(WIRELESS_DRIVER_PATH)/router/wlconf/
vpath %.c $(WIRELESS_DRIVER_PATH)/router/shared/
vpath %.c $(WIRELESS_DRIVER_PATH)/shared/bcmwifi/src/
endif

WLCONF_CFLAGS = $(filter-out -DDSLCPE,$(CFLAGS))
WLCONF_CFLAGS += -DDSLCPE_WLCONF_MAIN

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<


libwlmngr: $(OBJS) 
	$(LD) -shared -soname libwlmngr.so -o libwlmngr.so $(OBJS) -lc
#-lpthread
	install -m 755 libwlmngr.so $(INSTALL_DIR)/lib
	
#
# Implicit rule will make the .c into a .o
# Implicit rule is $(CC) -c $(CPPFLAGS) $(CFLAGS)
# See Section 10.2 of Gnu Make manual
# 
wlmngr: libwlmngr wldaemon.o
	$(CC) -o $@ wldaemon.o -s -Os -fomit-frame-pointer $(CMS_LIB_PATH) $(LIBS) -L$(LIBDIR) -L$(EXTRALIBDIR) -lgcc_s -lc

wlconf: libwlmngr $(WIRELESS_DRIVER_PATH)/router/wlconf/wlconf.c 
	$(CC) -c $(WLCONF_CFLAGS) -o wlconff.o $(WIRELESS_DRIVER_PATH)/router/wlconf/wlconf.c 
	$(CC) -o $@ wlconff.o -s -Os -fomit-frame-pointer $(CMS_LIB_PATH) $(LIBS) -L$(LIBDIR) -L$(EXTRALIBDIR)  -lgcc_s -lc

wlevt: wlevt.o  | libwlmngr
	$(CC) -o $@ $^ -s -Os -fomit-frame-pointer $(LIBS) $(CMS_LIB_PATH) -L$(LIBDIR) -L$(EXTRALIBDIR) -lcms_msg $(CMS_COMMON_LIBS) -lgcc_s -lc


#
# Include the rule for making dependency files.
# The '-' in front of the second include suppresses
# error messages when make cannot find the .d files.
# It will just regenerate them.
# See Section 4.14 of Gnu Make.
#

include $(BUILD_DIR)/make.deprules

#-include $(OBJS:.o=.d)
