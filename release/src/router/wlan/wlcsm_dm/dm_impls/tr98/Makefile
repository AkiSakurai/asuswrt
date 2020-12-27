#***********************************************************************
#
#  Copyright (c) 2006  Broadcom Corporation
#  All Rights Reserved
#
# 
# 
# This program is the proprietary software of Broadcom Corporation and/or its 
# licensors, and may only be used, duplicated, modified or distributed pursuant 
# to the terms and conditions of a separate, written license agreement executed 
# between you and Broadcom (an "Authorized License").  Except as set forth in 
# an Authorized License, Broadcom grants no license (express or implied), right 
# to use, or waiver of any kind with respect to the Software, and Broadcom 
# expressly reserves all rights in and to the Software and all intellectual 
# property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU HAVE 
# NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY NOTIFY 
# BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE. 
# 
# Except as expressly set forth in the Authorized License, 
# 
# 1. This program, including its structure, sequence and organization, 
#    constitutes the valuable trade secrets of Broadcom, and you shall use 
#    all reasonable efforts to protect the confidentiality thereof, and to 
#    use this information only in connection with your use of Broadcom 
#    integrated circuit products. 
# 
# 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS" 
#    AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR 
#    WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH 
#    RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND 
#    ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, 
#    FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR 
#    COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE 
#    TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF USE OR 
#    PERFORMANCE OF THE SOFTWARE. 
# 
# 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR 
#    ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, 
#    INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY 
#    WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN 
#    IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; 
#    OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE 
#    SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS 
#    SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY 
#    LIMITED REMEDY. 
#
#
#***********************************************************************/

#
# In most cases, you only need to modify this first section.
#

CURR_DIR := $(shell pwd)
BUILD_DIR:=$(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR:=$(word 1, $(BUILD_DIR))
OUTPUT_LIB:= libwlcsm_dm_tr98.so


WLCSM_FILES := wlcsm_dm_tr98.c wlmngr.c wlmdm.c  wlutil.c wldsltr.c nvram_mdm.c  wlsyscall.c 

#$(info $(WLCSM_FILES))

WLCSM_OBJS = $(foreach x, $(WLCSM_FILES),$(x:.c=.o))
OBJS += $(WLCSM_OBJS)

LIBS = -lwlcsm -lbcmshared
LIBS += -lcms_msg $(CMS_COMMON_LIBS) -lcms_dal -l cms_cli -lcms_msg $(CMS_COMMON_LIBS) -ldl $(CMS_CORE_LIBS)

all:$(OUTPUT_LIB) 
clean: generic_clean
	-rm -f *.o *.map *.d  *.d.*
	-rm -rf *.so
	
#
#
# Set our CommEngine directory (by splitting the pwd into two words
# at /userspace and taking the first word only).
# Then include the common defines under CommEngine.
#
BUILD_DIR:=$(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR:=$(word 1, $(BUILD_DIR))

include $(BUILD_DIR)/make.common


IMPL=$(shell readlink $(BUILD_DIR)/bcmdrivers/broadcom/net/wl/bcm9$(BRCM_CHIP) | sed 's/[^0-9]//g')
ifeq ($(IMPL),)
IMPL=0
endif
WL_SRC_BASE=$(shell if [ "$(IMPL)" -ge 22 ]; then echo /main/src; fi ;)

WIRELESS_DRIVER_PATH=$(BRCMDRIVERS_DIR)/broadcom/net/wl/bcm9$(BRCM_CHIP)$(WL_SRC_BASE)
INC_WIRELESS_DRIVER_PATH=$(WIRELESS_DRIVER_PATH)/include
COMM_WIRELESS_DRIVER_PATH=$(WIRELESS_DRIVER_PATH)/common/include
#
#
# Private apps and libs are allowed to include header files from the
# private and public directories.
#
# WARNING: Do not modify this section unless you understand the
# license implications of what you are doing.
#
ALLOWED_INCLUDE_PATHS := -I.\
                         -I./include \
                         -I../../include \
                         -I$(BUILD_DIR)/userspace/public/include  \
                         -I$(BUILD_DIR)/userspace/public/include/$(OALDIR) \
                         -I$(BUILD_DIR)/userspace/private/include  \
                         -I$(BUILD_DIR)/userspace/private/include/$(OALDIR) \
                         -I$(BUILD_DIR)/userspace/private/libs/wlcsm/include  \
			-I$(BUILD_DIR)/userspace/private/libs/cms_core \
			-I$(BUILD_DIR)/userspace/private/libs/wlcsm/include \
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
                         -I$(WIRELESS_DRIVER_PATH)/shared/bcmwifi/include \
                         -I$(WIRELESS_DRIVER_PATH)/wl/wlcsm_ext/include



CFLAGS +=  -DDSLCPE -DDSLCPE_WLCSM_EXT 
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

ifneq ($(strip $(MFP)),)
	CFLAGS += -DMFP
endif

                                                                                   


ifeq ($(DSLCPE_WLCSM_EXT),1)
	LIBS += -lwlcsm
	CFLAGS += -DDSLCPE_WLCSM_EXT
	ALLOWED_INCLUDE_PATHS += -I$(BUILD_DIR)/userspace/private/libs/wlcsm/include
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


# treat all warnings as errors
#CUSTOM_CFLAGS += -Wno-error=unused-function -Wno-error=unused-but-set-variable -Werror -Wfatal-errors  # NEW_FORBID_WARNINGS
CUSTOM_CFLAGS += -Werror  -Wunused-function -Wunused-variable
vpath %.c $(CURR_DIR)/src 

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<


$(OUTPUT_LIB): $(OBJS) 
	#$(CC) -s -Os -fomit-frame-pointer -shared -o $(OUTPUT_LIB) $(OBJS)
	$(CC)  -shared -o $(OUTPUT_LIB) $(OBJS)
	install -m 755 $(OUTPUT_LIB) $(INSTALL_DIR)/lib
install: all
	install -m 755 $(OUTPUT_LIB) $(INSTALL_DIR)/lib
	
	

#
# Include the rule for making dependency files.
# The '-' in front of the second include suppresses
# error messages when make cannot find the .d files.
# It will just regenerate them.
# See Section 4.14 of Gnu Make.
#

include $(BUILD_DIR)/make.deprules

-include $(OBJS:.o=.d)
