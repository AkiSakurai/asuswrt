# Makefile for hndrte based 43602a1 ROM Offload image building
#
# Copyright (C) 2012, Broadcom Corporation
# All Rights Reserved.
#
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#
# $Id: 43602a1-roml.mk$

####################################################################################################
# This makefile is used when building a ROM offload image, so an image that runs in RAM and calls
# routines that reside in ROM. It is not used when building a ROM. Default settings are defined in
# the 'wlconfig', settings in there may be redefined in this file when a 'default' ROM offload image
# should support more or less features than the ROM.
####################################################################################################

# chip specification
CHIP		:= 43602
ROMREV		:= a1
REV		:= a1
REVID		:= 1
# default targets
TARGETS		:= \
	pcie-ag-err-assert-splitrx \
	pcie-ag-mfgtest-seqcmds-splitrx-phydbg \
	pcie-ag-mfgtest-seqcmds-splitrx-phydbg-err-assert \
	pcie-ag-p2p-mchan-idauth-idsup-pno-aoe-pktfilter-pf2-keepalive-splitrx \
	pcie-ag-p2p-mchan-idauth-idsup-pno-aoe-pktfilter-pf2-keepalive-splitrx-err-assert \
	pcie-ag-p2p-mchan-idauth-idsup-pno-aoe-noe-ndoe-pktfilter-pf2-keepalive-splitrx-toe-ccx-mfp-anqpo-p2po-wl11k-wl11u-wnm-relmcast-txbf-fbt-tdls-sr-pktctx-amsdutx-proxd \
	pcie-ag-p2p-mchan-idauth-idsup-pno-aoe-noe-ndoe-pktfilter-pf2-keepalive-splitrx-toe-ccx-mfp-anqpo-p2po-wl11k-wl11u-wnm-relmcast-txbf-fbt-tdls-sr-pktctx-amsdutx-proxd-err-assert \
	pcie-ag-splitrx-fdap-mbss-mfp-wl11k-wl11u-wnm-txbf-pktctx-amsdutx-ampduretry-proptxstatus \
	pcie-ag-splitrx-fdap-mbss-mfp-wl11k-wl11u-wnm-txbf-pktctx-amsdutx-ampduretry-proptxstatus-err-assert

TEXT_START	:= 0x180000
DATA_START	:= 0x180200

# 640KB ROM
ROM_LOW		?= 0x00000000
ROM_HIGH	?= 0x000A0000

# common target attributes
TARGET_ARCH	:= arm
TARGET_CPU	:= cr4
TARGET_HBUS	:= pcie
THUMB		:= 1
HBUS_PROTO	:= msgbuf


# wlconfig & wltunable for rest of the build targets
WLCONFFILE	:= wlconfig_rte_43602a1_dev
WLTUNEFILE	:= wltunable_rte_43602a1.h

# ROM image info
ROMOFFLOAD	:= 1
ROMLDIR		:= $(TOPDIR)/../chipimages/$(CHIP)$(ROMREV)
ROMLLIB		:= roml.exe

# Use TCAM to patch ROM functions
TCAM		:= 1
JMPTBL_TCAM	:= 1
#JMPTBL_FULL	:= 1
GLOBALIZE	:= 1
#WLPATCHFILE	:= wlc_patch_43602a1.c
TCAM_PCNT	:= 1
TCAM_SIZE	:= 256

# features (sync with romlsym/43602a1.mk)
MEMBASE     := $(TEXT_START)
MEMSIZE		:= 983040
WLTINYDUMP	:= 0
DBG_ASSERT	:= 0
DBG_ASSERT_TRAP	:= 1
DBG_ERROR	:= 0
WLRXOV		:= 0

# Memory reduction features:
# - HNDLBUFCOMPACT: Compacts head/end pointers in lbuf to single word
#   To disable set HNDLBUFCOMPACT = 0
# - BCMPKTIDMAP: Suppresses pkt pointers to Ids in lbuf<next,link>, pktpool, etc
#   Must specify max number of packets (various pools + heap)

HNDLBUFCOMPACT	:= 1
BCMPKTIDMAP     := 1
BCMFRAGPOOL	:= 1
BCMLFRAG	:= 1
BCMSPLITRX	:= 1

POOL_LEN_MAX    := 256
POOL_LEN        := 10
WL_POST_FIFO1   := 2
MFGTESTPOOL_LEN := 10
FRAG_POOL_LEN	:= 256
RXFRAG_POOL_LEN	:= 192
PKT_MAXIMUM_ID  := 720

BSSCFG_ROM_COMPAT	:= 1

H2D_DMAQ_LEN	:= 256
D2H_DMAQ_LEN	:= 256

PCIE_NTXD	:= 256
PCIE_NRXD	:= 256

# ROM compatibility
BCM_ANQPO_ROM_COMPAT := 1

ifeq ($(findstring mfgtest,$(TARGET)),mfgtest)
	#allow MFG image to write OTP
	BCMNVRAMR	:= 0
	BCMNVRAMW	:= 1
endif

ifeq ($(findstring fdap,$(TARGET)),fdap)
	#router image, enable router specific features
	CLM_TYPE			:= 43602a1_access

	AP				:= 1
	SUFFIX_ENAB			:= 1
	INITDATA_EMBED			:= 1
	WLC_DISABLE_DFS_RADAR_SUPPORT	:= 0
	# Max MBSS virtual slave devices
ifeq ($(BSSCFG_ROM_COMPAT),1)
	MBSS_MAXSLAVES			:= 13
	EXTRA_DFLAGS	+= -DBSSCFG_ROM_COMPAT
else
	MBSS_MAXSLAVES			:= 8
endif
	# Max Tx Flowrings
	PCIE_TXFLOWS			:= 132

	# Reduce packet pool lens for internal assert
	# builds to fix out of memory issues
	ifeq ($(findstring assert,$(TARGET)),assert)
		FRAG_POOL_LEN	:= 160
		RXFRAG_POOL_LEN	:= 160
	endif
	WLBSSLOAD			:= 1
	WLOSEN				:= 1
	WLPROBRESP_MAC_FILTER		:= 1

	EXTRA_DFLAGS += -DPKTC_FDAP

	# Memory optimizations by avoiding unnecessary abandons
	# by disabling non-router related post-ROM code
	EXTRA_DFLAGS    += -DNO_ROAMOFFL_SUPPORT
	EXTRA_DFLAGS    += -DNO_WLNDOE_RA_SUPPORT
	EXTRA_DFLAGS	+= -DNO_GPIO_SUPP
else
	# CLM info
	CLM_TYPE	:= 43602a1
endif

# Avoid abandon due to debug print in non-err build
ifeq ($(findstring err,$(TARGET)),err)
	EXTRA_DFLAGS	+= -DADD_ERR_PRINT
endif

# Reduce stack size to increase free heap
HNDRTE_STACK_SIZE	:= 4608
EXTRA_DFLAGS		+= -DHNDRTE_STACK_SIZE=$(HNDRTE_STACK_SIZE)

# NDIS related
EXTRA_DFLAGS	+= -DIBSS_PEER_GROUP_KEY_DISABLED
EXTRA_DFLAGS	+= -DIBSS_PEER_DISCOVERY_EVENT_DISABLED
EXTRA_DFLAGS	+= -DIBSS_PEER_MGMT_DISABLED

EXTRA_DFLAGS	+= -DBCMPKTPOOL_ENABLED

# Add flops support
FLOPS_SUPPORT	:= 1

#Enable GPIO
EXTRA_DFLAGS	+= -DWLGPIOHLR

TOOLSVER	:= 2011.09
NOFNRENAME	:= 1

# Hard code some PHY characteristics to reduce RAM code size
# RADIO
EXTRA_DFLAGS	+= -DBCMRADIOREV=$(BCMRADIOREV)
EXTRA_DFLAGS	+= -DBCMRADIOVER=$(BCMRADIOVER)
EXTRA_DFLAGS	+= -DBCMRADIOID=$(BCMRADIOID)
# Only support EPA
EXTRA_DFLAGS	+= -DWLPHY_EPA_ONLY -DEPA_SUPPORT=1
# Don't support PAPD
EXTRA_DFLAGS	+= -DEPAPD_SUPPORT=0 -DWLC_DISABLE_PAPD_SUPPORT -DPAPD_SUPPORT=0

EXTRA_DFLAGS	+= -DAMPDU_SCB_MAX_RELEASE_AQM=32
EXTRA_DFLAGS	+= -DPKTC_DONGLE
EXTRA_DFLAGS	+= -DPCIEDEV_USE_EXT_BUF_FOR_IOCTL
PCIEDEV_MAX_IOCTLRSP_BUF_SIZE := 8192

# For CPU cycle count
# EXTRA_DFLAGS	+= -DBCMDBG_CPU

# Enable load average measuring
#EXTRA_DFLAGS	+= -DBCMDBG_LOADAVG -DBCMDBG_FORCEHT -DFIQMODE

# Disabled CCA_STATS post tapeout, need to fix up structures
EXTRA_DFLAGS	+= -DCCA_STATS_IN_ROM
EXTRA_DFLAGS	+= -DISID_STATS_IN_ROM

# ROM compatibility with legacy version of wlc_chanim_stats_t.
EXTRA_DFLAGS   += -DWLC_CHANIM_STATS_ROM_COMPAT2

# Flags to relocate struct fields and enum values that were excluded in ROMs,
# but are required in ROM offload builds.
EXTRA_DFLAGS	+= -DPROP_TXSTATUS_ROM_COMPAT -DWLMCHANPRECLOSE_ROM_COMPAT

# max fetch count at once
EXTRA_DFLAGS    += -DPCIEDEV_MAX_PACKETFETCH_COUNT=64
EXTRA_DFLAGS	+= -DPCIEDEV_MAX_LOCALITEM_COUNT=64
EXTRA_DFLAGS    += -DPD_NBUF_D2H_TXCPL=16
EXTRA_DFLAGS    += -DPD_NBUF_D2H_RXCPL=16
EXTRA_DFLAGS    += -DPD_NBUF_H2D_RXPOST=8
EXTRA_DFLAGS    += -DMAX_TX_STATUS_QUEUE=128
EXTRA_DFLAGS    += -DMAX_TX_STATUS_COMBINED=64

# RxOffsets for the PCIE mem2mem DMA
EXTRA_DFLAGS    += -DH2D_PD_RX_OFFSET=0
EXTRA_DFLAGS    += -DD2H_PD_RX_OFFSET=0

#Flag to relocate struct fields that were mismatched in ROM symbol stat table
EXTRA_DFLAGS    += -DWLBSSLOAD_ROM_COMPACT
