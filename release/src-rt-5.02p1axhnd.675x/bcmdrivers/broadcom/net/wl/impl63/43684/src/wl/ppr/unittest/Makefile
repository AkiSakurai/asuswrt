# Makefile for ppr unit test.
#
# Copyright (C) 2013, Broadcom Corporation
# All Rights Reserved.
#
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#
# $Id: Makefile xxxxxx 2013-11-14 06:00:44Z emanuell,shaib $

WLAN_ComponentsInUse := bcmwifi math
include ../../../makefiles/WLAN_Common.mk

#####################################################################################
##################### Following items require developer customization. ##############
#####################################################################################

# path of source file
SOURCE = ../src/wlc_ppr.c ../../../shared/bcmutils.c ../../../../components/math/src/bcm_math.c

# path of header file
INCLUDE = ../include ../../../include ../../../../components/shared $(WLAN_ComponentIncDirsR)

# path of unit test file
TEST_SRC = main.c test_ppr_size_routine.c test_ppr_clean.c

# Path to Check tool directory from the unit test source directory
CHECK_ROOT_PATH = ../../../unittest

#####################################################################################
##################### None customized parameter #####################################
#####################################################################################

# Path to central make file
include $(CHECK_ROOT_PATH)/unittest.mk
