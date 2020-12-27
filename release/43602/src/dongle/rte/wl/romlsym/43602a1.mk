# Makefile to generate romtable.S for 43602a1 ROM builds
#
# This makefile expects romctl.txt to exist in the roml directory
#
# Copyright (C) 2014, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#
# $Id: 43602a1.mk 266614 2011-06-14 22:16:03Z $:

####################################################################################################
# This makefile is used when building a ROM, specifically when generating a symbol table that
# represents the ROM contents.  It is not used when building a ROM offload image. The roml makefile
# (src/rte/roml/<chipid>/Makefile) inherits the settings in this romlsym file.
#
# Settings are defined in the 'wlconfig', settings in there should not be redefined in this file.
#
# The makefile generates "romtable_full.S", which is renamed to "romtable.S" when it is copied into
# the src/dongle/rte/roml/"chipidchiprev" directory.
####################################################################################################

# chip specification
CHIP		:= 43602
REV		:= a1
REVID		:= 1

TARGETS		:= pcie

# common target attributes
TARGET_HBUS	:= pcie
TARGET_ARCH	:= arm
TARGET_CPU	:= cr4
THUMB		:= 1
HBUS_PROTO	:= msgbuf
BAND		:= ag

# wlconfig & wltunable
WLCONFFILE	:= wlconfig_rte_43602a1_dev

# ROMCTL needed for location of romctl.txt
ROMCTL		:= $(TOPDIR)/../roml/$(CHIP)$(REV)/romctl.txt

ifeq ($(DEVSIM_BUILD),1)
#	WLTUNEFILE	:= wltunable_rte_$(DEVSIM_CHIP)$(DEVSIM_CHIP_REV).h
	ROMCTL		:= $(TOPDIR)/../roml/$(DEVSIM_CHIP)sim-$(CHIP)$(REV)/romctl.txt
	CHIP		:= $(DEVSIM_CHIP)
	DBG_ASSERT      := 1
else

WLTUNEFILE	:= wltunable_rte_43602a0.h
DBG_ASSERT      := 0
endif

# features (sync with rte/wl/current/43602a1-roml.mk)
MEMSIZE		:= 983040
MFGTEST		:= 0
WLTINYDUMP	:= 0

DBG_ERROR	:= 1
POOL_LEN_MAX	:= 60

VDEV		:= 1

# To limit PHY core checks
EXTRA_DFLAGS	+= -DBCMPHYCORENUM=$(BCMPHYCORENUM)
EXTRA_DFLAGS	+= -DBCMPHYCOREMASK=7

# CLM info
CLM_TYPE	:= 43602a1

# extra flags
#EXTRA_DFLAGS	+= -DSHARE_RIJNDAEL_SBOX	# Save 1400 bytes; wep & tkhash slightly slower
EXTRA_DFLAGS	+= -DAMPDU_RX_BA_DEF_WSIZE=64
EXTRA_DFLAGS	+= -DIBSS_PEER_GROUP_KEY_DISABLED
EXTRA_DFLAGS	+= -DIBSS_PEER_DISCOVERY_EVENT_DISABLED
EXTRA_DFLAGS	+= -DIBSS_PEER_MGMT_DISABLED

ifneq ($(ROMLIB),1)
ROMBUILD	:= 1
EXTRA_DFLAGS	+= -DBCMROMSYMGEN_BUILD
endif

#Enable GPIO
EXTRA_DFLAGS	+= -DWLGPIOHLR

TOOLSVER	:= 2011.09
NOFNRENAME	:= 1

BCMPKTPOOL	:= 1
BCMFRAGPOOL	:= 1
BCMLFRAG	:= 1
#FRAG_POOL_LEN should be less than POOL_LEN_MAX
FRAG_POOL_LEN   := 60
BCMPKTIDMAP     := 1
