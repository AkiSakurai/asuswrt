#
# Makefile for ROM libraries
#
# Copyright (C) 2014, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#
# $Id: Makefile 417004 2013-08-07 11:19:47Z $
#

# run silently by default
V ?= 0

# announce a few things everyone should know
ifeq ($(V),0)
export MAKE := $(MAKE) -s
endif

SUBMKFILES := $(wildcard */Makefile)
#SUBDIRS	:= $(sort $(SUBMKFILES:/Makefile=))
# SUBDIRS	:= 4324a0 4334b0
# SUBDIRS 	:= 43602a0

all: $(SUBDIRS)

$(SUBDIRS): FORCE
	@$(MAKE) -C $@

clean: $(SUBDIRS:=_clean)

$(SUBDIRS:=_clean): FORCE
	@$(MAKE) -C $(@:_clean=) clean

.PHONY: FORCE
FORCE:
