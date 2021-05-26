#
# Broadstream iQoS
#
# Broadcom Proprietary and Confidential. Copyright (C) 2016,
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom.
#
# $Id:$
#

TDTS_SHELL_DIR := $(CURDIR)/tdts/src/tdts_shell
TDTS_UDB_SHELL_DIR := $(CURDIR)/tdts/src/tdts_udb_shell
TDTS_UDBFW_DIR := $(CURDIR)/tdts/src/tdts_udbfw
TDTS_MODULES_DIR := $(CURDIR)/tdts/modules
TDTS_BIN_DIR := $(CURDIR)/tdts/bin
TDTS_OBJ_DIR := $(CURDIR)/tdts/lib

all: untar_tdts_obj tdts_shell tdts_udb_shell tdts_udbfw

untar_tdts_obj:
	@cd $(TDTS_OBJ_DIR); [ ! -e tdts_core.o ] && tar zxvf tdts_obj.tar.gz || true

tdts_shell: untar_tdts_obj
	$(MAKE) -C $(TDTS_SHELL_DIR)

tdts_udb_shell: tdts_shell
	$(MAKE) -C $(TDTS_UDB_SHELL_DIR)

tdts_udbfw: tdts_shell tdts_udb_shell
	$(MAKE) -C $(TDTS_UDBFW_DIR)

install: all
	install -d $(INSTALLDIR)/lib/modules/4.1.49/extra/
	install $(TDTS_SHELL_DIR)/tdts.ko $(INSTALLDIR)/lib/modules/4.1.49/extra/
	install $(TDTS_UDB_SHELL_DIR)/tdts_udb.ko $(INSTALLDIR)/lib/modules/4.1.49/extra/
	install $(TDTS_UDBFW_DIR)/tdts_udbfw.ko $(INSTALLDIR)/lib/modules/4.1.49/extra/
	install -d $(INSTALLDIR)/usr/sbin/
	install $(TDTS_BIN_DIR)/tdts_rule_agent $(INSTALLDIR)/usr/sbin/
	install $(TDTS_BIN_DIR)/setup.sh $(INSTALLDIR)/usr/sbin/
	install $(TDTS_BIN_DIR)/rule.trf $(INSTALLDIR)/usr/sbin/
	install $(TDTS_BIN_DIR)/sample.bin $(INSTALLDIR)/usr/sbin/
	install $(TDTS_BIN_DIR)/qos.conf $(INSTALLDIR)/usr/sbin/
	install $(TDTS_BIN_DIR)/moni.sh $(INSTALLDIR)/usr/sbin/
	install $(TDTS_BIN_DIR)/iqos-setup.sh $(INSTALLDIR)/usr/sbin/
	install $(TDTS_BIN_DIR)/clean-cache.sh $(INSTALLDIR)/usr/sbin/
	install $(TDTS_BIN_DIR)/shn_ctrl $(INSTALLDIR)/usr/sbin/
	install $(TDTS_BIN_DIR)/tcd $(INSTALLDIR)/usr/sbin/
	install $(TDTS_BIN_DIR)/tcd_monitor.sh $(INSTALLDIR)/usr/sbin/

clean:
	$(MAKE) -C $(TDTS_SHELL_DIR) clean
	$(MAKE) -C $(TDTS_UDB_SHELL_DIR) clean
	$(MAKE) -C $(TDTS_UDBFW_DIR) clean
	rm -f $(TDTS_MODULES_DIR)/Module.symvers
