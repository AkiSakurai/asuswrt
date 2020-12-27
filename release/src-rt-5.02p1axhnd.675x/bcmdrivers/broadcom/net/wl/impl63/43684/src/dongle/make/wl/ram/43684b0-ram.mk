# Makefile for hndrte based 43684b0 full ram Image
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
# $Id: 43684b0-ram.mk 778842 2019-09-11 12:05:57Z $

# chip specification
CHIP		:= 43684
REV		:= b0
# Undefine REVID to support both B0/B1 chip(rev 2/3) in this makefile
#REVID		:= 2

EXTRA_DFLAGS	+= -DSI_ENUM_BASE_DEFAULT=0x28000000
EXTRA_DFLAGS	+= -DSI_WRAP_BASE_DEFAULT=0x28100000
EXTRA_DFLAGS	+= -DSI_ARMCA7_RAM=0x00000000
EXTRA_DFLAGS	+= -DL2_PAGETABLE_ARRAY_SIZE=0xC00

# default targets
TARGETS		:= \
	pcie-mfgtest-seqcmds-phydbg \
	pcie-p2p-mchan-idauth-idsup-pno-aoe-pktfilter-pf2-keepalive \
	pcie-p2p-mchan-idauth-idsup-pno-aoe-noe-ndoe-pktfilter-pf2-keepalive-toe-ccx-mfp-anqpo-p2po-wl11k-wl11u-wnm-relmcast-txbf-fbt-tdls-sr-pktctx-amsdutx-proxd \
	pcie-fdap-mbss-mfp-wl11k-wl11u-wnm-txbf-pktctx-amsdutx-ampduretry-proptxstatus-chkd2hdma-11nprop-obss-dbwsw-ringer-dmaindex16-bgdfs-cevent-hostpmac-murx

TEXT_START	:= 0x0
DATA_START	:= 0x100

# common target attributes
TARGET_ARCH	:= arm
# actually, it is a ca7v2
TARGET_CPU	:= ca7
TARGET_HBUS	:= pcie
THUMB		:= 1
HBUS_PROTO	:= msgbuf

# wlconfig & wltunable for rest of the build targets
WLCONFFILE	:= wlconfig_rte_43684b0_dev
WLTUNEFILE	:= wltunable_rte_43684b0.h

# iDMA
PCIE_DMACHANNUM := 3

# features
MEMBASE		:= $(TEXT_START)
# 7MB of memory, reserve 1.5MB for BMC, so 5.5MB for SW
MEMSIZE		?= 5767168
WLTINYDUMP	:= 0
DBG_ASSERT	:= 0
DBG_ASSERT_TRAP	:= 1
DBG_ERROR	:= 1
WLRXOV		:= 0
PROP_TXSTATUS	:= 1
EXTRA_DFLAGS += -DPROP_TXSTATUS_SUPPR_WINDOW
# Memory reduction features:
# - HNDLBUFCOMPACT: Compacts head/end pointers in lbuf to single word
#   To disable set HNDLBUFCOMPACT = 0
# - BCMPKTIDMAP: Suppresses pkt pointers to Ids in lbuf<next,link>, pktpool, etc
#   Must specify max number of packets (various pools + heap)
# 43684b0 has >2M MEMSIZE, so need to disable HNDLBUFCOMPACT feature.
#
# When dhdhdr builds are used, DHD will insert SFH (LLCSNAP) and provide
# space for all MPDU's TxHeaders, upto a maximum of 4096 packets, as tracked by
# PKT_MAXIMUM_ID below.
#
HNDLBUFCOMPACT	:= 0
BCMPKTIDMAP     := 1
BCMFRAGPOOL	:= 1
BCMRXFRAGPOOL	:= 1
BCMLFRAG	:= 1
BCMSPLITRX	:= 1

DLL_USE_MACROS	:= 1
HNDLBUF_USE_MACROS := 1

POOL_LEN_MAX    := 2048
POOL_LEN        := 32
WL_POST_FIFO1   := 2
MFGTESTPOOL_LEN := 32
FRAG_POOL_LEN	:= 2048
RXFRAG_POOL_LEN	:= 512
PKT_MAXIMUM_ID  := 4096

# These definition could be adjust by HWA enabled.
# BCM_MAC_RXCPL_IDX_BITS is 11 bits, rxcpl_ptr[0] is not used.
# So max_rx_pkts limited to 2047
MAX_HOST_RXBUFS    := 2047
PD_NBUF_H2D_RXPOST := 16

# Split lbuf_frag control block and data buffer for tx lfrag pool
# Dongle has two types of data buffer D3 and D11
FRAG_D3_BUFFER_LEN := 512
# D11 buffer len should be half of FRAG_POOL_LEN at least.
FRAG_D11_BUFFER_LEN := 1024

# 96 bytes of lfrag struct + 36 bytes of PSM descriptor + 50 bytes of d11 header
EXTRA_DFLAGS	+= -DMAXPKTFRAGSZ=182

H2D_DMAQ_LEN	:= 256
D2H_DMAQ_LEN	:= 256

PCIE_NTXD	:= 256
PCIE_NRXD	:= 256

WL_SPLITRX_MODE	:= 4
WL_CLASSIFY_FIFO := 2

# PCIE IPC: Host PktId Audit support
#
# Only supported on STA BuPC, where DHD is using PKTID Mapper (DHD_PCIE_PKTID)
# - DHD (MAX_PKTID_ITEMS) and dongle (PCIEDEV_HOST_MAX_PKTID_ITEMS) must match
# Not applicable for Router: DHD Offload to Runner or when PktId is PktPtr)
#
ifeq ($(call opt,hpa), 1)
	EXTRA_DFLAGS	+= -DBCMPCIE_IPC_HPA
endif

# Support (Aggregated) Compact Work Items based messaging over PCIE IPC rev8
ifeq ($(call opt,acwi),1)
EXTRA_DFLAGS    += -DBCMPCIE_IPC_ACWI
endif

ifeq ($(call opt,hwa),1)
	# Must be in MODE 4
	WL_SPLITRX_MODE	:= 4

	# 43684B0 uses HWA2.1 SW Driver revision
	BCMHWA := 129

	# Support (Aggregated) Compact Work Items based messaging over PCIE IPC rev8
	EXTRA_DFLAGS    += -DBCMPCIE_IPC_ACWI

	# Need SBTOPCIE_INDICES support for RxPost rd_idx update by HWA.
ifneq ($(filter 1,$(call opt,hwa1ab) $(call opt,hwa2b) $(call opt,hwa3ab) $(call opt,hwa3a) $(call opt,hwa4a) $(call opt,hwa4b) $(call opt,hwaall)),)
	EXTRA_DFLAGS	+= -DSBTOPCIE_INDICES
endif # hwa1ab or hwa2b or hwa3ab or hwa4a or hwa4b or hwaall

	# Support 4 RX CPL channels.
	#EXTRA_DFLAGS    += -DRXCPL4

	# RX adjustment.
ifneq ($(filter 1,$(call opt,hwa1ab) $(call opt,hwaall)),)
	# The RXFRAG_POOL_LEN affect the RxBm buffer count as well.
ifeq ($(call opt,mfgtest),1)
	RXFRAG_POOL_LEN := 512
else
	RXFRAG_POOL_LEN := 1024
endif

	# Set RxBM maximum buffer count
	EXTRA_DFLAGS    += -DHWA_RXPATH_PKTS_MAX=$(RXFRAG_POOL_LEN)

	# Each tid per scb could hold up to 128 rxcpls.
	MAX_HOST_RXBUFS := 2047

	# XXX, We don't need PD_NBUF_H2D_RXPOST. (Cannot be zero, see pciedev_allocate_msgbuf_ring)
	PD_NBUF_H2D_RXPOST := 1
endif # hwa1ab or hwaall

	# TX adjustment.
ifneq ($(filter 1,$(call opt,hwa3ab) $(call opt,hwa3a) $(call opt,hwaall)),)
	# We don't need TX LFRAG and DHDHDR D3 Buffer.
	BCMFRAGPOOL     := 0
	EXTRA_DFLAGS    += -DBCM_DHDHDR_D3_DISABLE

	# Set TxBM maximum buffer count
	MAX_TXBM_BUFS   := 3072
	FRAG_D11_BUFFER_LEN := 1536
	EXTRA_DFLAGS    += -DHWA_TXPATH_PKTS_MAX=$(MAX_TXBM_BUFS)

	# For PKTID, POOL_LEN(32) + RXFRAG_POOL_LEN(1024) +
	# MAX_TXBM_BUFS(3072) + HEAP_PKT_MAXIMUM_ID(992) = 5120
	HEAP_PKT_MAXIMUM_ID := 992
	PKT_MAXIMUM_ID  := 5120
endif # hwa3ab or hwaall

	# HWA TX/RX need dhdhdr support
ifneq ($(filter 1,$(call opt,hwa1ab) $(call opt,hwa3ab) $(call opt,hwa3a) $(call opt,hwaall)),)
ifneq ($(call opt,dhdhdr),1)
	EXTRA_DFLAGS	+= -DBCM_DHDHDR
	EXTRA_DFLAGS	+= -DD3_BUFFER_LEN=$(FRAG_D3_BUFFER_LEN)
	EXTRA_DFLAGS	+= -DD11_BUFFER_LEN=$(FRAG_D11_BUFFER_LEN)
endif # dhdhdr

	# Use HWA related pkt macro on TX/RX data path
	EXTRA_DFLAGS    += -DHWA_PKT_MACRO
endif # hwa1ab or hwa3ab or hwaall

endif # hwa

ifeq ($(call opt,ate),1)
	TARGET_HBUS     := sdio
	CRC32BIN	:= 0
	EXTRA_DFLAGS	+= -DATE_43684_LOG
else
	# Enable ThreadX watchdog for non-ATE builds
	EXTRA_DFLAGS	+= -DTHREADX_WATCHDOG
	# Temporarily disable the use of the CC/PMU hardware watchdog (use ARM WD only)
	EXTRA_DFLAGS	+= -DTHREADX_WATCHDOG_HW_DISABLE
	# WATCHDOG_INTERVAL_MS determines the watchdog refresh frequency of the main thread as well as
	# the resolution of the timeout for other threads. Timeouts for other threads will be rounded
	# up to a multiple of the watchdog interval. Set to 0 to disable the watchdog for all threads.
	# WATCHDOG_TIMEOUT_MS is the timeout for the main thread.
	EXTRA_DFLAGS	+= -DWATCHDOG_INTERVAL_MS=2000 -DWATCHDOG_TIMEOUT_MS=5000

	# Increase console log buf size to 16*1024 bytes for all non-ATE images
	EXTRA_DFLAGS	+= -DBCM_BIG_LOG
endif

# Use ARM GT as hardware timer i.s.o. PMU timer
EXTRA_DFLAGS	+= -DTHREADX_ARM_GT_TIMER

EXTRA_DFLAGS    += -DBULKRX_PKTLIST -DSTS_FIFO_RXEN

# HWA TX is not ready for bulk dma
ifeq ($(filter 1,$(call opt,hwa3ab) $(call opt,hwaall)),)
	EXTRA_DFLAGS    += -DBULK_PKTLIST
endif # not hwa1ab or hwa3ab or hwaall

ifeq ($(call opt,mfgtest),1)
	#allow MFG image to write OTP
	BCMNVRAMR	:= 0
	BCMNVRAMW	:= 1
endif

ifeq ($(call opt,fdap),1)
	#router image, enable router specific features
	CLM_TYPE			:= 43684a0_access

	AP				:= 1
	SUFFIX_ENAB			:= 1
	WLC_DISABLE_DFS_RADAR_SUPPORT	:= 0
	# Max MBSS virtual slave devices
	MBSS_MAXSLAVES			:= 15

	#enable testbed AP by default for FDAP. In future create dedicated target for this.
	TESTBED_AP_11AX			:= 0

ifeq ($(call opt,sqs),1)
	# FD AP w/ SQS: Ucast flowrings are per TID per station
	# Max Tx Flowrings: (8 TID x 64 STAs) + (1 BCMC per 16 BSS)
	PCIE_TXFLOWS			:= 528
else
	# FD AP no SQS: Ucast flowrings are per AC per station
	# Max Tx Flowrings: (4 AC x 64 STAs) + (1 BCMC per 16 BSS)
	PCIE_TXFLOWS			:= 272
endif

	EXTRA_DFLAGS			+= -DPKTC_FDAP
else
	# CLM info
	CLM_TYPE			:= 43684a0
	# Enable the virtual device support for running DWDS on BUPC SoftAP
	# with the config_pcie_sta_xxxxx build
	EXTRA_DFLAGS			+= -DMAXVSLAVEDEVS=1
endif

BIN_TRX_OPTIONS_SUFFIX := -x 0x0 -x 0x0 -x 0x0

# Reduce stack size to increase free heap
HNDRTE_STACK_SIZE	:= 4608
EXTRA_DFLAGS		+= -DHNDRTE_STACK_SIZE=$(HNDRTE_STACK_SIZE)

# Set the maximum number of events targetting the main thread
# Memory cost is (MAIN_THREAD_EVENT_DEPTH * 4) bytes
MAIN_THREAD_EVENT_DEPTH := 128
EXTRA_DFLAGS		+= -DMAIN_THREAD_EVENT_DEPTH=$(MAIN_THREAD_EVENT_DEPTH)

# Add flops support
FLOPS_SUPPORT	:= 0

#Enable GPIO
EXTRA_DFLAGS	+= -DWLGPIOHLR

TOOLSVER	:= 2013.11
NOFNRENAME	:= 0
NOINLINE        :=

# Hard code some PHY characteristics to reduce RAM code size
# RADIO
EXTRA_DFLAGS	+= -DBCMRADIOREV=$(BCMRADIOREV)
EXTRA_DFLAGS	+= -DBCMRADIOVER=$(BCMRADIOVER)
EXTRA_DFLAGS	+= -DBCMRADIOID=$(BCMRADIOID)
# Only support EPA
EXTRA_DFLAGS	+= -DWLPHY_EPA_ONLY -DEPA_SUPPORT=1
# Don't support PAPD
EXTRA_DFLAGS	+= -DEPAPD_SUPPORT=0 -DWLC_DISABLE_PAPD_SUPPORT -DPAPD_SUPPORT=0
EXTRA_DFLAGS	+= -DBCMPHYCAL_CACHING

EXTRA_DFLAGS	+= -DAMPDU_SCB_MAX_RELEASE_AQM=64
# Set AMPDU_PKTQ_LEN to PKTQ_LEN_MAX
EXTRA_DFLAGS	+= -DAMPDU_PKTQ_LEN=65535
PKTC_DONGLE	:= 1
EXTRA_DFLAGS	+= -DPCIEDEV_USE_EXT_BUF_FOR_IOCTL
PCIEDEV_MAX_IOCTLRSP_BUF_SIZE := 8192

# Also increase H2DRING_RXPOST_MAX_ITEM to match WL_POST
EXTRA_DFLAGS	+= -DMAX_HOST_RXBUFS=$(MAX_HOST_RXBUFS)

#wowl gpio pin 14, Polarity at logic low is 1
WOWL_GPIOPIN	:= 0xe
WOWL_GPIO_POLARITY := 0x1
EXTRA_DFLAGS    += -DWOWL_GPIO=$(WOWL_GPIOPIN) -DWOWL_GPIO_POLARITY=$(WOWL_GPIO_POLARITY)

# max fetch count at once
EXTRA_DFLAGS    += -DPCIEDEV_MAX_PACKETFETCH_COUNT=64
EXTRA_DFLAGS	+= -DPCIEDEV_MAX_LOCALBUF_PKT_COUNT=512
EXTRA_DFLAGS    += -DPD_NBUF_D2H_TXCPL=32
EXTRA_DFLAGS    += -DPD_NBUF_D2H_RXCPL=32
# PD_NBUF_H2D_RXPOST * items(32) > MAX_HOST_RXBUFS for pciedev_fillup_haddr=>pciedev_get_host_addr callback from pktpool_get
EXTRA_DFLAGS    += -DPD_NBUF_H2D_RXPOST=$(PD_NBUF_H2D_RXPOST)
EXTRA_DFLAGS    += -DMAX_TX_STATUS_QUEUE=256
EXTRA_DFLAGS    += -DMAX_TX_STATUS_COMBINED=128

# Size of local queue to store completions
EXTRA_DFLAGS    += -DPCIEDEV_CNTRL_CMPLT_Q_SIZE=16

# Support for SROM format
EXTRA_DFLAGS	+= -DBCMPCIEDEV_SROM_FORMAT

# Support for sliding window within flowrings
# This allows an option to set a large flowring size, but operate in a sliding
# window model where dongle only consumes packets upto the window size.
#EXTRA_DFLAGS    += -DFLOWRING_SLIDING_WINDOW -DFLOWRING_SLIDING_WINDOW_SIZE=512

# Support for using smaller bitsets on each flowring - instead of the full flowring depth
EXTRA_DFLAGS    += -DFLOWRING_USE_SHORT_BITSETS

# Enabled BCMDBG for QT bring-up
#EXTRA_DFLAGS	+= -DBCMDBG -DBCMDBG_BOOT

EXTRA_DFLAGS	+= -DLARGE_NVRAM_MAXSZ=8192

# Do not enable the Event pool for 43684b0
EXTRA_DFLAGS	+= -DEVPOOL_SIZE=0

# Define MAXPKTDATABUFSZ for RX buffer size; need to accommodate extra rx headroom(216) + rx control offset(40) + max size MPDU
EXTRA_DFLAGS	+= -DMAXPKTDATABUFSZ=1840

#EXTRA_DFLAGS    += -DSBTOPCIE_INDICES

ifeq ($(call opt,hdmaaddr64),1)
EXTRA_DFLAGS	+= -DHOST_DMA_ADDR64
endif

ifeq ($(call opt,cu),1)
# HND CPU Utilization Tool for Threadx. When activated disabled ARM LowPowerMode
EXTRA_DFLAGS	+= -DHND_CPUUTIL
endif

ifneq (,$(filter 1,$(call opt,buzzz) $(call opt,buzzz_kpi) $(call opt,buzzz_func) $(call opt,cu)))
# Cycles per microsec is equivalent to CPU speed in units of MHz
EXTRA_DFLAGS	+= -DCYCLES_PER_USEC=1500
endif

EXTRA_DFLAGS	+= -DWLPROBRESP_INTRANSIT_FILTER

# Correct ether lenght field of HW converted dot3 header after tailing out ICV/MIC
EXTRA_DFLAGS	+= -DBCM43684_HDRCONVTD_ETHER_LEN_WAR

ifeq ($(call opt,splitrxmode2),1)
	# Multiplying factor of x4; 64 bytes of copycount
	COPY_CNT_BYTES	:= 16
	EXTRA_DFLAGS	+= -DMAXPKTRXFRAGSZ=360
else
	# Multiplying factor of x4; 16 bytes of copycount
	COPY_CNT_BYTES	:= 4
	# 12bytes [RPH] + 12bytes SWRxstatus + 40bytesHWRxstatus + 14bytes plcp + 4bytes pad + 50bytes d11 hdr + 16 bytes copy count  =  148
	# Extra 96 bytes for Rxlfrag data structure + 148 = 244bytes.
	EXTRA_DFLAGS	+= -DMAXPKTRXFRAGSZ=244
endif
ifeq ($(call opt,hostdpi),1)
	EXTRA_DFLAGS	+= -DHOSTDPI
endif

# Set default amsdu_aggsf value to 4.
EXTRA_DFLAGS	+= -DMAX_TX_SUBFRAMES_ACPHY=4
