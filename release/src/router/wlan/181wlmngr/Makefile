#
# In most cases, you only need to modify this first section.
#
default:all

WLAN_USERSPACE_CFG = 1

CURR_DIR := $(shell pwd)
BUILD_DIR:=$(HND_SRC)
include $(BUILD_DIR)/make.common

CSM_HEADER := $(BUILD_DIR)/bcmdrivers/broadcom/char/wlcsm_ext/bcm9$(BRCM_CHIP)/include/wlcsm_linux.h
IMPL=$(shell readlink $(BUILD_DIR)/bcmdrivers/broadcom/net/wl/bcm9$(BRCM_CHIP) | sed 's/[^0-9]//g')
WL_IMPL_PLUS= $(strip $(shell if [ "$(IMPL)" -ge 24 ]; then echo Y; fi ;))
WL_SRC_BASE=$(shell if [ "$(IMPL)" -ge 22 ]; then echo /main/src; fi ;)
WIRELESS_DRIVER_PATH=$(BRCMDRIVERS_DIR)/broadcom/net/wl/bcm9$(BRCM_CHIP)$(WL_SRC_BASE)
INC_WIRELESS_DRIVER_PATH=$(WIRELESS_DRIVER_PATH)/include
COMM_WIRELESS_DRIVER_PATH=$(WIRELESS_DRIVER_PATH)/common/include
-include $(BUILD_DIR)/bcmdrivers/broadcom/net/wl/impl$(IMPL)$(WL_SRC_BASE)/wl/config/wlconfig_lx_wl_dslcpe


EXE  = wlmngr2  wlevt2
OBJS = wlmngr.o  wlsyscall.o wlutil.o  wlmngr_specialvar_handlers.o wlmngr_wps_var_handlers.o 
OBJS += wl.o wl_linux.o shutils.o wlconf.o defaults.o bcmwifi_channels.o nvparse.o

PURE181=$(or $(strip $(BUILD_DM_PURE181)),$(strip $(BUILD_DM_DETECT)))

ifneq ($(strip $(BUILD_BRCM_CMS)),)
LIBS = -lcms_msg $(CMS_COMMON_LIBS) -lcms_dal -l cms_cli -lcms_msg $(CMS_COMMON_LIBS) -ldl $(CMS_CORE_LIBS)
else
LIBS = -ldl 
endif
#LIBS += -lwlctl -lnvram -lwlmngr
LIBS += -lwlctl -lnvram -lpthread -lm  -lwlbcmshared

#all dynamic: libwlmngr2 $(EXE) 
all dynamic: $(EXE) 

install: all
	install -m 755 $(EXE) $(INSTALL_DIR)/bin


clean: generic_clean
	rm -f $(INSTALL_DIR)/bin/$(EXE) $(INSTALL_DIR)/lib/libwlmngr2.so libwlmngr2.so
	-rm -f *.o
	-rm -f $(EXE)


#
# Set our CommEngine directory (by splitting the pwd into two words
# at /userspace and taking the first word only).
# Then include the common defines under CommEngine.
#
CFLAGS += $(SSP_TYP_COMPILER_OPTS)
LIBS += $(SSP_TYP_LIBS)
SSP_LIB_FLAGS = $(SSP_TYP_LIBS) $(SSP_LIB_INC)

#
# Private apps and libs are allowed to include header files from the
# private and public directories.
#
# WARNING: Do not modify this section unless you understand the
# license implications of what you are doing.
#
ALLOWED_INCLUDE_PATHS := -I.\
                         -I../wlmngr  \
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

LIBS += -lwlcsm -lwlcsm_dm 

ifneq ($(strip $(BUILD_BRCM_CMS)),)
ifneq ($(strip $(PURE181)),)
LIBS += -lwlcsm_dm_tr181
CFLAGS += -DPURE181
else
ifneq ($(strip $(BUILD_UNIFIED_WLMNGR)),)
LIBS += -lwlcsm_dm_tr98
endif
endif
else
LIBS += -lwlcsm_dm_nocms
CFLAGS += -DNO_CMS
endif
CFLAGS += -DDSLCPE_WLCSM_EXT
ALLOWED_INCLUDE_PATHS += -I$(WIRELESS_DRIVER_PATH)/wl/wlcsm_ext/include
ALLOWED_INCLUDE_PATHS += -I$(BUILD_DIR)/userspace/private/libs/wlcsm/include
ALLOWED_INCLUDE_PATHS += -I$(BUILD_DIR)/userspace/private/apps/wlan/wlcsm_dm/include

ifneq ($(strip $(WL_IMPL_PLUS)),)
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

ifneq ($(strip $(BUILD_IEEE1905)),)
CFLAGS += -DDSLCPE_1905
ALLOWED_INCLUDE_PATHS += -I$(BRCMDRIVERS_DIR)/broadcom/net/wl/bcm9$(BRCM_CHIP)$(WL_SRC_BASE)/wps/brcm_apps/include
endif

ifneq ($(strip $(MFP)),)
CFLAGS += -DMFP
endif

# treat all warnings as errors
#CUSTOM_CFLAGS += -Wno-error=unused-function -Wno-error=unused-but-set-variable -Werror -Wfatal-errors  # NEW_FORBID_WARNINGS

vpath %.c $(WIRELESS_DRIVER_PATH)/router/wlconf/
vpath %.c $(WIRELESS_DRIVER_PATH)/router/shared/
vpath %.c $(WIRELESS_DRIVER_PATH)/shared/bcmwifi/src/

WLCONF_CFLAGS = $(filter-out -DDSLCPE,$(CFLAGS))
WLCONF_CFLAGS += -DDSLCPE_WLCONF_MAIN

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<


libwlmngr2: $(OBJS) $(CSM_HEADER)
	$(CC) -shared -o libwlmngr2.so $(OBJS)
	install -m 755 libwlmngr2.so $(INSTALL_DIR)/lib
	
#
# Implicit rule will make the .c into a .o
# Implicit rule is $(CC) -c $(CPPFLAGS) $(CFLAGS)
# See Section 10.2 of Gnu Make manual
# 


wlmngr2: libwlmngr2 wldaemon.o
	$(CC) -o $@ wldaemon.o $(OBJS) -s -Os -fomit-frame-pointer $(CMS_LIB_PATH) $(LIBS) -L$(LIBDIR) -L$(EXTRALIBDIR) -lc -lgcc_s 
	#$(CC) -o $@ wldaemon.o $(OBJS) -fomit-frame-pointer $(CMS_LIB_PATH) $(LIBS) -L$(LIBDIR) -L$(EXTRALIBDIR) -lc -lgcc_s 


wlevt2: wlevt.o
ifneq ($(strip $(BUILD_BRCM_CMS)),)
	$(CC) -o $@ $^ -s -Os -fomit-frame-pointer $(CMS_LIB_PATH) -L$(LIBDIR) -L$(EXTRALIBDIR) -lwlcsm -lc -lgcc_s -lcms_msg $(CMS_COMMON_LIBS) $(SSP_LIB_FLAGS)
else
	$(CC) -o $@ $^ -s -Os -fomit-frame-pointer $(CMS_LIB_PATH) -L$(LIBDIR) -L$(EXTRALIBDIR) -lwlcsm -lc -lgcc_s  $(SSP_LIB_FLAGS)
endif


#
# Include the rule for making dependency files.
# The '-' in front of the second include suppresses
# error messages when make cannot find the .d files.
# It will just regenerate them.
# See Section 4.14 of Gnu Make.
#

include $(BUILD_DIR)/make.deprules

#-include $(OBJS:.o=.d)
