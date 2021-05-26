# Makefile bor hndrte based 4365c0 full ram Image
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
# $Id: 4365c0-ram.mk 784323 2020-02-25 16:59:50Z $

# chip specification
CHIP		:= 4365
ROMREV		:= c0
REV		:= c0
REVID		:= 4
# default targets
TARGETS		:= \
	pcie-err-assert-splitrx \
	pcie-mfgtest-seqcmds-splitrx-phydbg \
	pcie-mfgtest-seqcmds-splitrx-phydbg-err-assert \
	pcie-p2p-mchan-idauth-idsup-pno-aoe-pktfilter-pf2-keepalive-splitrx \
	pcie-p2p-mchan-idauth-idsup-pno-aoe-pktfilter-pf2-keepalive-splitrx-err-assert \
	pcie-p2p-mchan-idauth-idsup-pno-aoe-noe-ndoe-pktfilter-pf2-keepalive-splitrx-toe-ccx-mfp-anqpo-p2po-wl11k-wl11u-wnm-relmcast-txbf-fbt-tdls-sr-pktctx-amsdutx-proxd \
	pcie-p2p-mchan-idauth-idsup-pno-aoe-noe-ndoe-pktfilter-pf2-keepalive-splitrx-toe-ccx-mfp-anqpo-p2po-wl11k-wl11u-wnm-relmcast-txbf-fbt-tdls-sr-pktctx-amsdutx-proxd-err-assert \
	pcie-splitrx-fdap-mbss-mfp-wl11k-wl11u-wnm-txbf-pktctx-amsdutx-ampduretry-proptxstatus \
	pcie-splitrx-fdap-mbss-mfp-wl11k-wl11u-wnm-txbf-pktctx-amsdutx-ampduretry-proptxstatus-err-assert

TEXT_START	:= 0x200000

# common target attributes
TARGET_ARCH	:= arm
TARGET_CPU	:= ca7
TARGET_HBUS	:= pcie
THUMB		:= 1
HBUS_PROTO	:= msgbuf

# wlconfig & wltunable for rest of the build targets
WLCONFFILE	:= wlconfig_rte_4365c0_bu
WLTUNEFILE	:= wltunable_rte_4365c0.h

# features (sync with romlsym/4365c0.mk)
MEMBASE     := $(TEXT_START)
MEMSIZE		?= 2228224
WLTINYDUMP	:= 0
DBG_ASSERT	:= 0
DBG_ASSERT_TRAP	:= 1
DBG_ERROR	:= 1
WLRXOV		:= 0
PROP_TXSTATUS	:= 1

# Memory reduction features:
# - BCMPKTIDMAP: Suppresses pkt pointers to Ids in lbuf<next,link>, pktpool, etc
#   Must specify max number of packets (various pools + heap)
#
# When dhdhdr builds are used, DHD will insert SFH (LLCSNAP) and provide
# space for all MPDU's TxHeaders, upto a maximum of 2560 packets, as tracked by
# PKT_MAXIMUM_ID below.
#
BCMPKTIDMAP     := 1
BCMFRAGPOOL	:= 1
BCMRXFRAGPOOL	:= 1
BCMLFRAG	:= 1
BCMSPLITRX	:= 1

POOL_LEN_MAX    := 512
POOL_LEN        := 10
WL_POST_FIFO1   := 2
MFGTESTPOOL_LEN := 10
FRAG_POOL_LEN	:= 256
RXFRAG_POOL_LEN	:= 192
PKT_MAXIMUM_ID  := 1024

# Split lbuf_frag control block and data buffer for tx lfrag pool
# Dongle has two types of data buffer D3 and D11
FRAG_D3_BUFFER_LEN := 128
FRAG_D11_BUFFER_LEN := 128

# By default MAXPKTFRAGSZ is 338 and the PKTTAILROOM will be not enough in TKIP
# case. Add more 4-bytes for C0 here.
EXTRA_DFLAGS    += -DMAXPKTFRAGSZ=342

H2D_DMAQ_LEN	:= 256
D2H_DMAQ_LEN	:= 256

PCIE_NTXD	:= 256
PCIE_NRXD	:= 256

WL_CLASSIFY_FIFO := 2
EXTRA_DFLAGS    += -DFORCE_RX_FIFO1

ifeq ($(call opt,ate),1)
	TARGET_HBUS     := sdio
	CRC32BIN	:= 0
else
EXTRA_DFLAGS    += -DBULKRX_PKTLIST
EXTRA_DFLAGS    += -DBULK_PKTLIST
endif

ifeq ($(call opt,mfgtest),1)
	#allow MFG image to write OTP
	BCMNVRAMR	:= 0
	BCMNVRAMW	:= 1
endif

ifeq ($(call opt,fdap),1)
	#router image, enable router specific features
	CLM_TYPE			:= 4365a0_access

	AP				:= 1
	SUFFIX_ENAB			:= 1
	INITDATA_EMBED			:= 1
	WLC_DISABLE_DFS_RADAR_SUPPORT	:= 0
	# Max MBSS virtual slave devices
	MBSS_MAXSLAVES			:= 4
	# Max Tx Flowrings
#	PCIE_TXFLOWS			:= 132

	# Reduce packet pool lens for internal assert
	# builds to fix out of memory issues
	ifeq ($(call opt,assert),1)
		FRAG_POOL_LEN		:= 512
		RXFRAG_POOL_LEN		:= 160
	endif
	WLBSSLOAD			:= 1
	WLOSEN				:= 1
	WLPROBRESP_MAC_FILTER		:= 1

	EXTRA_DFLAGS	+= -DPKTC_FDAP

	# enlarge MEMSIZE to 2M+256K for fdap firmware
	MEMSIZE                         := 2359296

	# Memory optimizations by avoiding unnecessary abandons
	# by disabling non-router related post-ROM code
	#EXTRA_DFLAGS    += -DNO_ROAMOFFL_SUPPORT
	#EXTRA_DFLAGS    += -DNO_WLNDOE_RA_SUPPORT
else
	# CLM info
	CLM_TYPE	:= 4365a0_access

	# use default MEMSIZE 2M+128K and disable CT-DMA amd MU-TX
	MEMSIZE                         := 2228224

	EXTRA_DFLAGS    += -DBCM_DMA_CT_DISABLED
	EXTRA_DFLAGS    += -DWL_MU_TX_DISABLED
endif

BIN_TRX_OPTIONS_SUFFIX := -x 0x0 -x 0x0 -x 0x0

# Reduce stack size to increase free heap
HNDRTE_STACK_SIZE	:= 4608
EXTRA_DFLAGS		+= -DHNDRTE_STACK_SIZE=$(HNDRTE_STACK_SIZE)

EXTRA_DFLAGS	+= -DBCMPKTPOOL_ENABLED

# Add flops support
FLOPS_SUPPORT	:= 1

#Enable GPIO
EXTRA_DFLAGS	+= -DWLGPIOHLR

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

EXTRA_DFLAGS	+= -DAMPDU_SCB_MAX_RELEASE_AQM=32
PKTC_DONGLE	:= 1
EXTRA_DFLAGS	+= -DPCIEDEV_USE_EXT_BUF_FOR_IOCTL

# Ideal: (MAX_HOST_RXBUFS > (RXFRAG_POOL_LEN + POOL_LEN)); At least (MAX_HOST_RXBUFS > (WL_POST + WL_POST_FIFO1)) for pciedev_fillup_rxcplid callback from pktpool_get
# Also increase H2DRING_RXPOST_MAX_ITEM to match WL_POST
EXTRA_DFLAGS	+= -DMAX_HOST_RXBUFS=512

#wowl gpio pin 14, Polarity at logic low is 1
WOWL_GPIOPIN	:= 0xe
WOWL_GPIO_POLARITY := 0x1
EXTRA_DFLAGS    += -DWOWL_GPIO=$(WOWL_GPIOPIN) -DWOWL_GPIO_POLARITY=$(WOWL_GPIO_POLARITY)

# max fetch count at once
EXTRA_DFLAGS    += -DPCIEDEV_MAX_PACKETFETCH_COUNT=64
EXTRA_DFLAGS	+= -DPCIEDEV_MAX_LOCALITEM_COUNT=64
EXTRA_DFLAGS    += -DPD_NBUF_D2H_TXCPL=16
EXTRA_DFLAGS    += -DPD_NBUF_D2H_RXCPL=16
# PD_NBUF_H2D_RXPOST * items(32) > MAX_HOST_RXBUFS for pciedev_fillup_haddr=>pciedev_get_host_addr callback from pktpool_get
EXTRA_DFLAGS    += -DPD_NBUF_H2D_RXPOST=16
EXTRA_DFLAGS    += -DMAX_TX_STATUS_QUEUE=128
EXTRA_DFLAGS    += -DMAX_TX_STATUS_COMBINED=64

# Support for SROM format
EXTRA_DFLAGS	+= -DBCMPCIEDEV_SROM_FORMAT

EXTRA_DFLAGS	+= -DWLP2P_DISABLED -DWLMCHAN_DISABLED -DWL_RELMCAST_DISABLED -DP2PO_DISABLED -DANQPO_DISABLED

EXTRA_DFLAGS	+= -DLARGE_NVRAM_MAXSZ=8192

# Do not enable the Event pool for 4366c0/4365c0
EXTRA_DFLAGS	+= -DEVPOOL_SIZE=0

#EXTRA_DFLAGS    += -DSBTOPCIE_INDICES

# Only for Runner development, in preparation for 43684. Not for submission into
# 10.10/7.35 . 4366c0 will be used for Runner development to support Aggregated
# Compact Work Items based messaging, prior to 43684 availability.
ifeq ($(call opt,acwi),1)
EXTRA_DFLAGS    += -DBCMPCIE_IPC_ACWI
endif

ifeq ($(call opt,dhdhdr),1)
# DHDHDR only works with Dual Pkt AMSDU optimization.
EXTRA_DFLAGS	+= -DDUALPKTAMSDU
endif

# Instead of disabling frameburst completly in dynamic frame burst logic,
# we enable RTS/CTS in frameburst.
EXTRA_DFLAGS	+= -DFRAMEBURST_RTSCTS_PER_AMPDU

# To tune frameburst override thresholds
EXTRA_DFLAGS	+= -DTUNE_FBOVERRIDE

ifeq ($(call opt,cu),1)
# HND CPU Utilization Tool for Threadx. When activated disabled ARM LowPowerMode
EXTRA_DFLAGS    += -DHND_CPUUTIL
endif

ifneq (,$(filter 1,$(call opt,buzzz) $(call opt,buzzz_func) $(call opt,cu)))
# Cycles per microsec is equivalent to CPU speed in units of MHz
EXTRA_DFLAGS	+= -DCYCLES_PER_USEC=800
endif
