# Makefile for hndrte based 43602a3 ROM Offload image building
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
ROMREV		:= a3
REV		:= a3
REVID		:= 3
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
CLM_INC_FILE_SUFFIX	:= _$(CHIP)$(REV)_inc

# wlconfig & wltunable for rest of the build targets
WLCONFFILE	:= wlconfig_rte_43602a3_dev
WLTUNEFILE	:= wltunable_rte_43602a3.h

# ROM image info
ROMOFFLOAD	:= 1
ROMLDIR		:= $(TOPDIR)/../chipimages/$(CHIP)$(ROMREV)
ROMLLIB		:= roml.exe

# Use TCAM to patch ROM functions
TCAM		:= 1
JMPTBL_TCAM	:= 1
#JMPTBL_FULL	:= 1
GLOBALIZE	:= 1
#WLPATCHFILE	:= wlc_patch_43602a3.c
TCAM_PCNT	:= 1
TCAM_SIZE	:= 256

# features (sync with romlsym/43602a3.mk)
MEMBASE     := $(TEXT_START)
MEMSIZE		:= 983040
WLTINYDUMP	:= 0
DBG_ASSERT	:= 0
DBG_ASSERT_TRAP	:= 1
DBG_ERROR	:= 0
WLRXOV		:= 0
DBG_TEMPSENSE	:= 1

# Memory reduction features:
# - HNDLBUFCOMPACT: Compacts head/end pointers in lbuf to single word
#   To disable set HNDLBUFCOMPACT = 0
# - BCMPKTIDMAP: Suppresses pkt pointers to Ids in lbuf<next,link>, pktpool, etc
#   Must specify max number of packets (various pools + heap)

HNDLBUFCOMPACT	:= 0
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

H2D_DMAQ_LEN	:= 256
D2H_DMAQ_LEN	:= 256

PCIE_NTXD	:= 256
PCIE_NRXD	:= 256

ifeq ($(call opt,mfgtest),1)
	#allow MFG image to write OTP
	BCMNVRAMR	:= 0
	BCMNVRAMW	:= 1
endif

ifeq ($(call opt,fdap),1)
	#router image, enable router specific features
	CLM_TYPE			:= 43602a3_access

	AP				:= 1
	SUFFIX_ENAB			:= 1
	WLC_DISABLE_DFS_RADAR_SUPPORT	:= 0
	# Max MBSS virtual slave devices
	MBSS_MAXSLAVES			:= 8
	# Max Tx Flowrings, 8+3*maxSTA, maxSTA=60
	PCIE_TXFLOWS			:= 188
	WL_NTXD				:= 256
	# Reduce packet pool lens for internal assert
	# builds to fix out of memory issues
	ifeq ($(call opt,assert),1)
		FRAG_POOL_LEN	:= 160
		RXFRAG_POOL_LEN	:= 160
		FRAG_D3_BUFFER_LEN  := 80
		FRAG_D11_BUFFER_LEN := 80
		DBG_SUSPEND_MAC := 1
		EXTRA_DFLAGS    += -DDBG_PHYTXERR
		# Reduce bcmc hw fifo size below the nr of d11 bufs
		EXTRA_DFLAGS    += -DTX_BCMC_FIFO_REDUCTION_FACTOR=4
	else
		FRAG_POOL_LEN			:= 256
		FRAG_D3_BUFFER_LEN		:= 128
		#boost performance for 43602a3 AP (from default 96, costs 8K)
		FRAG_D11_BUFFER_LEN := 128
		# Reduce bcmc hw fifo size below the nr of d11 bufs
		EXTRA_DFLAGS    += -DTX_BCMC_FIFO_REDUCTION_FACTOR=2
	endif

	# In router, we would "never" be able to support flowring per tid.
	EXTRA_DFLAGS	+= -DFLOW_PRIO_MAP_AC

	EXTRA_DFLAGS	+= -DPKTC_FDAP
	EXTRA_DFLAGS	+= -DAMPDU_COMPATIBILITY

	# Enable AIRTIES_MESH flag to inlcude Airties mesh changes
	EXTRA_DFLAGS	+= -DAIRTIES_MESH

	# Memory optimizations by avoiding unnecessary abandons
	# by disabling non-router related post-ROM code

        # For AP specific FBT changes
	FBT_FDAP	:= 1
else
	# not supported in a3 (?)
	CLM_TYPE	:= 43602a3
endif

#CLM_BLOBS += 43602a1
#CLM_BLOBS += 43602a1_access
#CLM_BLOBS += 43602_sig

# Reduce stack size to increase free heap
HNDRTE_STACK_SIZE	:= 4608
EXTRA_DFLAGS		+= -DHNDRTE_STACK_SIZE=$(HNDRTE_STACK_SIZE)

EXTRA_DFLAGS	+= -DBCMPKTPOOL_ENABLED

# Add flops support
FLOPS_SUPPORT	:= 1

#Enable GPIO
EXTRA_DFLAGS	+= -DWLGPIOHLR

# Ideal: (MAX_HOST_RXBUFS > (RXFRAG_POOL_LEN + POOL_LEN)); At least
# (MAX_HOST_RXBUFS > (WL_POST + WL_POST_FIFO1)) for pciedev_fillup_rxcplid callback from pktpool_get
# Also increase H2DRING_RXPOST_MAX_ITEM to match WL_POST
EXTRA_DFLAGS	+= -DMAX_HOST_RXBUFS=512

#wowl gpio pin 14, Polarity at logic low is 1
WOWL_GPIOPIN = 0xe
WOWL_GPIO_POLARITY = 0x1
EXTRA_DFLAGS    += -DWOWL_GPIO=$(WOWL_GPIOPIN) -DWOWL_GPIO_POLARITY=$(WOWL_GPIO_POLARITY)

TOOLSVER	:= 2013.11
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
EXTRA_DFLAGS	+= -DROMMED_PHY_43602a3

EXTRA_DFLAGS	+= -DAMPDU_SCB_MAX_RELEASE_AQM=32
EXTRA_DFLAGS	+= -DPCIEDEV_USE_EXT_BUF_FOR_IOCTL
PCIEDEV_MAX_IOCTLRSP_BUF_SIZE := 8192

# For CPU cycle count
# EXTRA_DFLAGS	+= -DBCMDBG_CPU

# Enable load average measuring
#EXTRA_DFLAGS	+= -DBCMDBG_LOADAVG -DBCMDBG_FORCEHT -DFIQMODE

# max fetch count at once
EXTRA_DFLAGS    += -DPCIEDEV_MAX_PACKETFETCH_COUNT=64
EXTRA_DFLAGS	+= -DPCIEDEV_MAX_LOCALBUF_PKT_COUNT=256

EXTRA_DFLAGS	+= -DMAX_TX_STATUS_BUF_LEN=224
EXTRA_DFLAGS    += -DMAX_TX_STATUS_COMBINED=64

EXTRA_DFLAGS	+= -DMAX_RXCPL_BUF_LEN=256

# PD_NBUF_H2D_RXPOST * items(32) > MAX_HOST_RXBUFS for pciedev_fillup_haddr=>pciedev_get_host_addr
# callback from pktpool_get
EXTRA_DFLAGS    += -DPD_NBUF_H2D_RXPOST=16

# RxOffsets for the PCIE mem2mem DMA
EXTRA_DFLAGS    += -DH2D_PD_RX_OFFSET=0
EXTRA_DFLAGS    += -DD2H_PD_RX_OFFSET=0

# Set deadman timeout to 5 seconds
EXTRA_DFLAGS    += -DDEADMAN_TIMEOUT=800000000

# Remove unused fields from structs to reduce memory footprint
EXTRA_DFLAGS	+= -DAMPDU_NO_NON_AQM -DAMPDU_REMOVE_OBSOLETE_FIELDS
EXTRA_DFLAGS	+= -DAMSDU_REMOVE_OBSOLETE_FIELDS

EXTRA_DFLAGS	+= -DWME_PER_AC_MAXRATE_DISABLE

#Support for SROM format
EXTRA_DFLAGS	+= -DBCMPCIEDEV_SROM_FORMAT

# Min SCB alloc memory limit - 16KB
EXTRA_DFLAGS    += -DMIN_SCBALLOC_MEM=16384

# 43602 128MByte Small PCIE Address Region 1 (64MB per SBTOPCIE0 and SBTOPCIE1)
# 0x08000000 - 0x0FFFFFFF
EXTRA_DFLAGS	+= -DPCIE_ADDR_MATCH1=0x08000000

# SCBTAG disabled for now, could be enabled as tput optimization for host-side SCBs
# >0 = Perform SCBTAG caching
# 1  = Use SCBTAG cached values (EXT build)
# 2  = Access host scb each time, and audit against SCBTAG cached values (DBG)
#
ifeq ($(call opt,assert),1)
#EXTRA_DFLAGS	+= -DBCM_HOST_MEM_SCBTAG=2
else
#EXTRA_DFLAGS	+= -DBCM_HOST_MEM_SCBTAG=1
endif
# Cache a host-allocated SCB_TXC_INFO cubby
#EXTRA_DFLAGS	+= -DBCM_HOST_MEM_TXCTAG

# Support for sliding window within flowrings
# This allows an option to set a large flowring size, but operate in a sliding
# window model where dongle only consumes packets upto the window size.
EXTRA_DFLAGS    += -DFLOWRING_SLIDING_WINDOW -DFLOWRING_SLIDING_WINDOW_SIZE=512

# Support for using smaller bitsets on each flowring - instead of the full flowring depth
EXTRA_DFLAGS    += -DFLOWRING_USE_SHORT_BITSETS

# Enable speed optimization in AMSDU
EXTRA_DFLAGS	+= -DDUALPKTAMSDU

# Ensure no mismatch with ROM for LBUF struct (DHDHDR related)
EXTRA_DFLAGS	+= -DBCM_DHDHDR_IN_ROM

# Disable/enable AMSDU for AC_VI
EXTRA_DFLAGS    += -DDISABLE_AMSDUTX_FOR_VI

# Instead of disabling frameburst completly in dynamic frame burst logic, we enable RTS/CTS in frameburst.
EXTRA_DFLAGS    += -DFRAMEBURST_RTSCTS_PER_AMPDU

# To tune frameburst override thresholds
EXTRA_DFLAGS    += -DTUNE_FBOVERRIDE

# Enable Probe response intransit filter, to limit to 1 probe response per station DA.
EXTRA_DFLAGS	+= -DWLPROBRESP_INTRANSIT_FILTER

#Manual patching
WLPATCHFILE		:= wlc_patch_43602a3.c
PATCHSIGN_ENABLED	:= 1
