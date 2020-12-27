#
# FCBS ROM and metadata make file
#
# Generates the following files:
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
# $Id: $
#
# ROML build:
#         - FCBS ROM
#         - FCBS Metadata file
#
# ROM offload build:
#         - FCBS RAM data file (FCBS ROM patches)
#         - FCBS Metadata file
#
# RAM build:
#         - FCBS RAM data file
#         - FCBS Metadata file
#

FCBS_DIR = $(SRCBASE)/tools/fcbs/

ifdef ROMTBL_VARIANT
	# ROML build
	FCBS_CFLAGS = -DFCBS_ROMLIB
else ifeq ($(ROMOFFLOAD),1)
	# ROM offload build
	FCBS_CFLAGS = -DFCBS_ROMOFFLOAD
else
	# RAM build
	FCBS_CFLAGS = -DFCBS_RAM_BUILD
endif

FCBS_CFLAGS += -DFCBS_ROM_BUILD

#ifdef UNRELEASEDCHIP
FCBS_CFLAGS += -DUNRELEASEDCHIP
#endif // endif

vpath fcbs_%.c $(SRCBASE)/tools/fcbs/
vpath fcbs_%.c $(SRCBASE)/../components/chips/fcbs/$(CHIP)/$(REV)
vpath %.c $(SRCBASE)/../components/phy/ac/dsi/
vpath %.c $(SRCBASE)/../components/phy/ac/papdcal/
vpath %.c $(SRCBASE)/wl/sys/

FCBS_INCLUDE = -I $(SRCBASE)/include/
FCBS_INCLUDE += -I $(SRCBASE)/../components/phy/ac/include/
FCBS_INCLUDE += -I $(SRCBASE)/../components/phy/ac/dsi/
FCBS_INCLUDE += -I $(SRCBASE)/../components/phy/ac/papdcal/
FCBS_INCLUDE += -I $(SRCBASE)/../components/phy/old
FCBS_INCLUDE += -I $(SRCBASE)/wl/sys/
FCBS_INCLUDE += -I $(SRCBASE)/../components/phy/cmn/include/
FCBS_INCLUDE += -I $(SRCBASE)/../components/phy/cmn/hal/
FCBS_INCLUDE += -I $(SRCBASE)/../components/phy/cmn/core/
FCBS_INCLUDE += -I $(SRCBASE)/../components/phy/cmn/temp/
FCBS_INCLUDE += -I $(SRCBASE)/wl/ppr/include/
FCBS_INCLUDE += -I $(SRCBASE)/wl/dump/include/
FCBS_INCLUDE += -I $(SRCBASE)/wl/iocv/include/
FCBS_INCLUDE += -I $(SRCBASE)/wl/chctx/include/
FCBS_INCLUDE += -I $(SRCBASE)/../components/shared/
FCBS_INCLUDE += -I $(SRCBASE)/../components/wlioctl/include/
FCBS_INCLUDE += -I $(SRCBASE)/../components/proto/include/
FCBS_INCLUDE += -I $(SRCBASE)/shared/bcmwifi/include/
FCBS_INCLUDE += -I $(SRCBASE)/shared/
FCBS_INCLUDE += -I .

FCBS_CFILES = fcbsutils.c fcbs_rom.c
FCBS_CFILES += fcbs_input_sdio.c fcbs_input_initvals.c phy_ac_dsi_data.c fcbs_input_ctrl.c d11ucode_ulp.c phy_ac_papdcal_data.c

FCBS_OBJS = $(FCBS_CFILES:.c=.fcbs.o)

FCBS_CHIP_DIR = $(SRCBASE)/../components/chips/images/fcbs/$(CHIP)/$(REV)/

# This argument order should not be changed becuase, fcbsrom utility
# expects the output files in the same order.
FCBS_UTIL_ARGS = $(FCBS_CHIP_DIR)/fcbs_input.bin
FCBS_UTIL_ARGS += $(FCBS_CHIP_DIR)/fcbs_input_metadata.bin
FCBS_UTIL_ARGS += $(FCBS_CHIP_DIR)/fcbs_rom_metadata.bin
FCBS_UTIL_ARGS += $(FCBS_CHIP_DIR)/fcbs_rom.bin
FCBS_UTIL_ARGS += $(FCBS_CHIP_DIR)/fcbs_input_seq_cnt.bin

FCBS_ROM_OUTPUT_FILES = fcbs_rom.bin fcbs_metadata.bin fcbs_metadata.c fcbs_input.bin fcbs_input_metadata.bin fcbs_input_seq_cnt.bin

%.fcbs.o: %.c wlconf.h d11shm.h
	gcc -c $(FCBS_CFLAGS) $(FCBS_INCLUDE) $< -o $@

fcbsrom: $(FCBS_OBJS)
	gcc $(FCBS_OBJS) -o $@

fcbs_metadata.bin: fcbsrom
	./$< --ram-data=fcbs_ram_data.bin -o $@ -- $(FCBS_UTIL_ARGS)

fcbs_ram_data.c: fcbs_metadata.c
	bin2c $(@:.c=.bin) $@ $(@:.c=)

fcbs_metadata.c: fcbs_metadata.bin
	bin2c $< $@ $(@:.c=)
ifdef ROMTBL_VARIANT
	cp $(FCBS_ROM_OUTPUT_FILES) $(FCBS_CHIP_DIR)
	cp $(FCBS_CHIP_DIR)/fcbs_metadata.bin $(FCBS_CHIP_DIR)/fcbs_rom_metadata.bin
	cp $(FCBS_CHIP_DIR)/fcbs_metadata.c $(FCBS_CHIP_DIR)/fcbs_rom_metadata.c
endif
