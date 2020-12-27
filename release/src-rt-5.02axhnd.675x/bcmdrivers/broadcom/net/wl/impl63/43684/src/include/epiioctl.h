/*
 * iLine10(tm) Windows device driver custom OID definitions.
 *
 * Copyright 2019 Broadcom
 *
 * This program is the proprietary software of Broadcom and/or
 * its licensors, and may only be used, duplicated, modified or distributed
 * pursuant to the terms and conditions of a separate, written license
 * agreement executed between you and Broadcom (an "Authorized License").
 * Except as set forth in an Authorized License, Broadcom grants no license
 * (express or implied), right to use, or waiver of any kind with respect to
 * the Software, and Broadcom expressly reserves all rights in and to the
 * Software and all intellectual property rights therein.  IF YOU HAVE NO
 * AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
 * WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
 * THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1. This program, including its structure, sequence and organization,
 * constitutes the valuable trade secrets of Broadcom, and you shall use
 * all reasonable efforts to protect the confidentiality thereof, and to
 * use this information only in connection with your use of Broadcom
 * integrated circuit products.
 *
 * 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 * "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
 * REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 * OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
 * DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 * NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 * ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 * OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 * BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
 * SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
 * IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 * IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
 * ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
 * OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 * NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: epiioctl.h 674525 2016-12-09 04:17:19Z $
 */

#ifndef _epiioctl_h_
#define	_epiioctl_h_

/*
 * custom OID support
 *
 * 0xFF - implementation specific OID
 * 0xDA - first byte of Epigram PCI vendor ID
 * 0xFE - second byte of Epigram PCI vendor ID
 * 0xXX - the custom OID number
 *
 * Order is monotonically increasing but non-contiguous
 * to preserve backwards compatibility with older OIDS.
 *
 * NOTE: Add any query oids to IS_QUERY_OID() .
 */

#define OID_EPI_BASE				0xFFFEDA00

#define	OID_EPI_LIST				(OID_EPI_BASE + 0x01)
#define	OID_EPI_UP				(OID_EPI_BASE + 0x02)
#define	OID_EPI_DOWN				(OID_EPI_BASE + 0x03)
#define	OID_EPI_LOOP				(OID_EPI_BASE + 0x04)
#define	OID_EPI_DUMP				(OID_EPI_BASE + 0x05)
#define	OID_EPI_TXPE				(OID_EPI_BASE + 0x06)
#define	OID_EPI_TXBPB				(OID_EPI_BASE + 0x06) /* usb driver compatibility */
#define	OID_EPI_TXPRI				(OID_EPI_BASE + 0x07)
#define	OID_EPI_SETMSGLEVEL			(OID_EPI_BASE + 0x08)
#define	OID_EPI_PROMISC				(OID_EPI_BASE + 0x0A)
#define	OID_EPI_LINKINT				(OID_EPI_BASE + 0x0B)

#define	OID_EPI_PESSET				(OID_EPI_BASE + 0x0C)
#define	OID_EPI_DUMPPES				(OID_EPI_BASE + 0x0D)

#define	OID_EPI_SET_DEBUG			(OID_EPI_BASE + 0x10)

#define	OID_LARQ_DUMP				(OID_EPI_BASE + 0x11)
#define	OID_LARQ_ONOFF				(OID_EPI_BASE + 0x12)
#define	OID_LARQ_SETMSGLEVEL			(OID_EPI_BASE + 0x13)

#define OID_USB_DUMP				(OID_EPI_BASE + 0x14)
#define	OID_EPI_MAXRXBPB			(OID_EPI_BASE + 0x15)

#define	OID_EPISTAT_BLOCK			(OID_EPI_BASE + 0x16)
#define	OID_EPISTAT_BLOCKSIZE			(OID_EPI_BASE + 0x17)
#define	OID_EPISTAT_GET_CHANNEL_ESTIMATE	(OID_EPI_BASE + 0x18)
#define	OID_EPISTAT_BLOCK_LARQ			(OID_EPI_BASE + 0x19)
#define	OID_EPISTAT_BLOCKSIZE_LARQ		(OID_EPI_BASE + 0x1a)

#define	OID_EPI_TXDOWN				(OID_EPI_BASE + 0x1b)

#define OID_USB_VERSION				(OID_EPI_BASE + 0x1c)

#define	OID_EPI_PROMISCTYPE			(OID_EPI_BASE + 0x1d)
#define	OID_EPI_TXGEN				(OID_EPI_BASE + 0x1e)

#define	OID_EL_SETMSGLEVEL			(OID_EPI_BASE + 0x21)
#define	OID_EL_LARQ_ONOFF			(OID_EPI_BASE + 0x24)
#define	OID_EPI_GET_INSTANCE			(OID_EPI_BASE + 0x25)
#define	OID_EPI_IS_EPILAYER			(OID_EPI_BASE + 0x26)
#define	OID_CSA					(OID_EPI_BASE + 0x28)

#define	OID_SET_HPNA_MODE			(OID_EPI_BASE + 0x2c)
#define OID_SET_CSA_HPNA_MODE			(OID_EPI_BASE + 0x2d)

#define	OID_EPI_RXSEL_HYST			(OID_EPI_BASE + 0x2e)
#define	OID_EPI_RXSEL_MAXERR			(OID_EPI_BASE + 0x2f)
#define	OID_EPI_RXSEL_MAXDELTA			(OID_EPI_BASE + 0x30)
#define	OID_EPI_RXSEL_KMAX			(OID_EPI_BASE + 0x31)

#define	OID_EPI_DUMPSCB				(OID_EPI_BASE + 0x33)

#define OID_EPI_XLIST				(OID_EPI_BASE + 0x34)

#define OID_EPI_COS_MODE			(OID_EPI_BASE + 0x36)
#define OID_EPI_COS_LIST			(OID_EPI_BASE + 0x37)
#define OID_EPI_DUMPPM				(OID_EPI_BASE + 0x38)
#define OID_EPI_FORCEPM				(OID_EPI_BASE + 0x39)

/* Srom access for (at least) USB */
#define	OID_EPI_READ_SROM			(OID_EPI_BASE + 0x3a)
#define	OID_EPI_WRITE_SROM			(OID_EPI_BASE + 0x3b)

#define OID_EPI_CONSUMECERT			(OID_EPI_BASE + 0x3c)
#define OID_EPI_SENDUPLINK			(OID_EPI_BASE + 0x3d)
/* (OID_EPI_BASE + 0x3e-0x3f) are defined in oidencap.h */

/* Classify oids */

#define IS_EPI_OID(oid) (((oid) & 0xFFFFFF00) == 0xFFFEDA00)

#define	IS_QUERY_OID(oid) \
	((oid == OID_EPI_LIST) || \
	 (oid == OID_EPI_XLIST) || \
	 (oid == OID_EPI_DUMP) || \
	 (oid == OID_EPI_DUMPSCB) || \
	 (oid == OID_EPISTAT_BLOCK) || \
	 (oid == OID_EPISTAT_BLOCKSIZE) || \
	 (oid == OID_EPISTAT_BLOCK_LARQ) || \
	 (oid == OID_EPISTAT_BLOCKSIZE_LARQ) || \
	 (oid == OID_EPI_GET_INSTANCE) || \
	 (oid == OID_USB_DUMP) || \
	 (oid == OID_USB_VERSION) || \
	 (oid == OID_LARQ_DUMP) || \
	 (oid == OID_EPI_READ_SROM) || \
	 (oid == OID_EPI_DUMPPES) || \
	 (oid == OID_EPI_DUMPPM) || \
	 (oid == OID_EPI_FORCEPM))
#endif /* _epiioctl_h_ */
