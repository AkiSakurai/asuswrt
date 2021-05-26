#
# Makefile for 43602a1 ROM library build
#
# Copyright 2020 Broadcom
#
# This program is the proprietary software of Broadcom and/or
# its licensors, and may only be used, duplicated, modified or distributed
# pursuant to the terms and conditions of a separate, written license
# agreement executed between you and Broadcom (an "Authorized License").
# Except as set forth in an Authorized License, Broadcom grants no license
# (express or implied), right to use, or waiver of any kind with respect to
# the Software, and Broadcom expressly reserves all rights in and to the
# Software and all intellectual property rights therein.  IF YOU HAVE NO
# AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
# WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
# THE SOFTWARE.
#
# Except as expressly set forth in the Authorized License,
#
# 1. This program, including its structure, sequence and organization,
# constitutes the valuable trade secrets of Broadcom, and you shall use
# all reasonable efforts to protect the confidentiality thereof, and to
# use this information only in connection with your use of Broadcom
# integrated circuit products.
#
# 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
# "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
# REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
# OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
# DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
# NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
# ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
# CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
# OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
#
# 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
# BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
# SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
# IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
# IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
# ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
# OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
# NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
#
# $Id: Makefile 268359 2011-06-22 08:58:52Z $:
#

####################################################################################################
# this file is used when building a ROM. It defines ROM settings in addition to the (wireless
# oriented) ROM features listed in src/wl/config/wlconfig_rte_<chipid>_dev (referred to as the
# 'wlconfig'), and the settings defined in src/dongle/rte/wl/romlsym/43602a1.mk (referred to as the
# 'romlsym'). This file should not redefine any wlconfig nor romlsym Makefile variable.
####################################################################################################

####################################################################################################
# There is a set of parameters (examples: POOL_LEN, BUS_NRXD, etc) that are typically tweaked after
# tapeout. Tweaking of this set must not cause any abandons in the ROM offload image.
# To verify that the ROM was indeed build to conform, these parameters are not specified in the ROM
# build process (so not in this nor the romlsym makefile), so they will assume their default values.
# The desired parameter value is then specified in the ROM offload makefile, so the developer can
# verify then if the the ROM offload makefile results in 0 abandons before releasing the ROM bits.
####################################################################################################

# top level make
#
SRCBASE = $(shell /bin/pwd)/../../../..

ROMPFX		:= H3E_L40RMHD16384X64R612VXVTSSY_L

CHIP		:= 43602
CHIPREV		:= a1
REVID		:= 1

# 43602 has no bootloader
#PREPEND_BINARY := 1

# ROM size limit
# 640KB, 6 banks of 128KB each.
ROMSIZE		:= 655360
ROMBANKSIZE	:= 131072

# ROM library start address, allow 32 bytes space for FLOPS
TEXT_START	:= 0x20

# shdat_start will be located on RAM address DATA_START. RAM runs between 0x0018_0000..0x0027_0000.
DATA_START	:= 0x00180200

# Romtable generation
ROMTBL_CFG	:= 43602a1
ROMTBL_VARIANT	?= pcie

MEM_WIDTH	:= 8
# 43602 ROM bank size is 128KB, ROM is 64 bits -> 16384 64 bits words -> 0x4000
ROMADDR_QT	:= 3fff
# ROM is 64 bits wide
QT_MEM_WIDTH	:= 64

# Add flops support
FLOPS_SUPPORT := 1

# Use TCAM (... entries) to patch ROM functions
#TCAM		:= 1 don't define for ROM builds
JMPTBL_TCAM	:= 1
#JMPTBL_FULL	:= 1
#GENROMTBL	:= 1
TCAM_SIZE	:= 256
TCAM_PCNT	:= 1

TOOLSVER	:= 2011.09
NOFNRENAME	:= 1

# this set of flags is normally only specified in the 'current' dir
EXTRA_DFLAGS	+= -DBCMRADIOREV=$(BCMRADIOREV)
EXTRA_DFLAGS	+= -DBCMRADIOVER=$(BCMRADIOVER)
EXTRA_DFLAGS	+= -DBCMRADIOID=$(BCMRADIOID)
EXTRA_DFLAGS	+= -DAMPDU_SCB_MAX_RELEASE_AQM=40
EXTRA_DFLAGS	+= -DWLFCHOST_TRANSACTION_ID
# packet chaining (pktc)
EXTRA_DFLAGS	+= -DPKTC_DONGLE

# include RTE makefile configuration
include $(SRCBASE)/dongle/rte/roml/Makefile.cmn
