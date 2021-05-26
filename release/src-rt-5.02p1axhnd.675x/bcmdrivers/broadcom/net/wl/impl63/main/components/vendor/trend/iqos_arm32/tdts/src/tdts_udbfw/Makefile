#
# Copyright 2014 Trend Micro Incorporated
# Redistribution and use in source and binary forms, with or without modification, 
# are permitted provided that the following conditions are met:
# 1. Redistributions of source code must retain the above copyright notice, 
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice, 
#    this list of conditions and the following disclaimer in the documentation 
#    and/or other materials provided with the distribution.
# 3. Neither the name of the copyright holder nor the names of its contributors 
#    may be used to endorse or promote products derived from this software without 
#    specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
# IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
# OF SUCH DAMAGE.
#

#####
TDTS_DIR_PACK = $(TDTS_KMOD_DIR)/../..
TDTS_DIR_PACK_MOD = $(TDTS_DIR_PACK)/modules
TDTS_DIR_PACK_LIB = $(TDTS_DIR_PACK)/lib

core_mod_name := tdts_udb_core.o
#####

#
# CMD_FORBID_EMPTY_STR
# Compare two strings. Exit -1 if it's not equal.
# 1: input symbol (left value)
# 2: value of input symbol (right value)
# 3: Some more comment to display as error msg.
#
CMD_FORBID_EMPTY_STR = if [ "$(2)" = "" ]; then echo " * ERROR: Input string $(1) is empty." "$(3)"; exit -1; fi;

#
# CMD_MAKE_LINUX - A wrapper to run make on linux kernel (support cross compile)
# 1: Linux kernel source directory
# 2: This module directory
# 3: Kernel arch
# 4: Cross compiler prefix
# 5: Verbose level
# 6: Makefile target
#
CMD_MAKE_LINUX = $(MAKE) -C "$(1)" M=$(2) $(if $(3),ARCH=$(3)) $(if $(4),CROSS_COMPILE=$(4)) $(if $(5),V=$(5)) $(6)

#####

mod_name := tdts_udbfw
mod_path := $(mod_name).ko

ifeq ($(KERNELRELEASE),) # Check if goals are from kernel or not.

export TDTS_KMOD_DIR ?= $(CURDIR)

include Makefile.cfg

all: prepare verify_kernel_dir kmod_build

.PHONY: prepare
prepare:
	@if [ -f $(TDTS_DIR_PACK_LIB)/$(core_mod_name) ]; then ln -sf $(TDTS_DIR_PACK_LIB)/$(core_mod_name) . ; fi


.PHONY: verify_kernel_dir
verify_kernel_dir:
	@$(call CMD_FORBID_EMPTY_STR,TMCFG_KERN_DIR,$(TMCFG_KERN_DIR))
	@echo "...verify kernel directory: $(TMCFG_KERN_DIR) with arch $(TMCFG_KERN_ARCH)"
	@test -d $(TMCFG_KERN_DIR)

.PHONY: kmod_build
kmod_build: verify_kernel_dir
	@echo "...build kernel module at kernel dir '$(TMCFG_KERN_DIR)' from $(CURDIR)"
	@$(call CMD_MAKE_LINUX,$(TMCFG_KERN_DIR),$(CURDIR),$(TMCFG_KERN_ARCH),$(TMCFG_TC_PFX),$(if $(TMCFG_DBG_VERBOSE_CC_MSG),1),modules)

.PHONY: clean
clean:
	@echo "...clean object files by kernel dir '$(TMCFG_KERN_DIR)'"
	@$(call CMD_MAKE_LINUX,$(TMCFG_KERN_DIR),$(CURDIR),$(TMCFG_KERN_ARCH),$(TMCFG_TC_PFX),$(if $(TMCFG_DBG_VERBOSE_CC_MSG),1),clean)

else
#
# This's kbuild.
#

include $(TDTS_KMOD_DIR)/Makefile.cfg

obj-m := $(mod_name).o

KBUILD_EXTRA_SYMBOLS := $(TDTS_DIR_PACK_MOD)/../src/tdts_shell/Module.symvers
KBUILD_EXTRA_SYMBOLS += $(TDTS_DIR_PACK_MOD)/../src/tdts_udb_shell/Module.symvers

include $(TDTS_KMOD_DIR)/kbuild.inc

$(mod_name)-objs += $(tdts-udbfw-obj-y)

ccflags-y += $(TMCFG_E_EXTRA_CFLAGS)
ccflags-y += -I$(TDTS_DIR_PACK)/include/udb/core
ccflags-y += -I$(TDTS_DIR_PACK)/include/udb/shell
ccflags-y += $(TMCFG_APP_K_EXTRA_CFLAGS)

endif

