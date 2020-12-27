/*
 *  Routines to access SPROM and to parse SROM/CIS variables.
 *
 * Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: bcmsrom.c,v 1.336.8.71 2011-01-28 00:42:43 Exp $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <stdarg.h>
#include <bcmutils.h>
#include <hndsoc.h>
#include <sbchipc.h>
#include <bcmdevs.h>
#include <bcmendian.h>
#include <sbpcmcia.h>
#include <pcicfg.h>
#include <siutils.h>
#include <bcmsrom.h>
#include <bcmsrom_tbl.h>
#ifdef BCMSPI
#include <spid.h>
#endif

#include <bcmnvram.h>
#include <bcmotp.h>

#if defined(BCMUSBDEV)
#include <sbsdio.h>
#include <sbhnddma.h>
#include <sbsdpcmdev.h>
#endif

#if defined(WLTEST) || defined(DHD_SPROM) || defined(BCMDBG)
#include <sbsprom.h>
#endif
#include <proto/ethernet.h>	/* for sprom content groking */


#if defined(BCMDBG_ERR) || defined(WLTEST)
#define	BS_ERROR(args)	printf args
#else
#define	BS_ERROR(args)
#endif

#define SROM_OFFSET(sih)  ((sih->ccrev > 31) ? \
	(((sih->cccaps & CC_CAP_SROM) == 0) ? NULL : \
	 ((uint8 *)curmap + PCI_16KB0_CCREGS_OFFSET + CC_SROM_OTP)) : \
	((uint8 *)curmap + PCI_BAR0_SPROM_OFFSET))

#if defined(WLTEST) || defined(DHD_SPROM) || defined(BCMDBG)
#define WRITE_ENABLE_DELAY	500	/* 500 ms after write enable/disable toggle */
#define WRITE_WORD_DELAY	20	/* 20 ms between each word write */
#endif

typedef struct varbuf {
	char *base;		/* pointer to buffer base */
	char *buf;		/* pointer to current position */
	unsigned int size;	/* current (residual) size in bytes */
} varbuf_t;
extern char *_vars;
extern uint _varsz;

#define SROM_CIS_SINGLE	1


static int initvars_srom_si(si_t *sih, osl_t *osh, void *curmap, char **vars, uint *count);
static void _initvars_srom_pci(uint8 sromrev, uint16 *srom, uint off, varbuf_t *b);
static int initvars_srom_pci(si_t *sih, void *curmap, char **vars, uint *count);
static int initvars_cis_pcmcia(si_t *sih, osl_t *osh, char **vars, uint *count);
#if !defined(BCMUSBDEV)
static int initvars_flash_si(si_t *sih, char **vars, uint *count);
#endif 
#ifdef BCMSPI
static int initvars_cis_spi(osl_t *osh, char **vars, uint *count);
#endif /* BCMSPI */
static int sprom_cmd_pcmcia(osl_t *osh, uint8 cmd);
static int sprom_read_pcmcia(osl_t *osh, uint16 addr, uint16 *data);
#if defined(WLTEST) || defined(DHD_SPROM) || defined(BCMDBG)
static int sprom_write_pcmcia(osl_t *osh, uint16 addr, uint16 data);
#endif 
static int sprom_read_pci(osl_t *osh, si_t *sih, uint16 *sprom, uint wordoff, uint16 *buf,
                          uint nwords, bool check_crc);
#if defined(BCMNVRAMW) || defined(BCMNVRAMR)
static int otp_read_pci(osl_t *osh, si_t *sih, uint16 *buf, uint bufsz);
#endif /* defined(BCMNVRAMW) || defined(BCMNVRAMR) */
static uint16 srom_cc_cmd(si_t *sih, osl_t *osh, void *ccregs, uint32 cmd, uint wordoff,
                          uint16 data);

static int initvars_table(osl_t *osh, char *start, char *end, char **vars, uint *count);
static int initvars_flash(si_t *sih, osl_t *osh, char **vp, uint len);

#if defined(BCMUSBDEV)
static int get_si_pcmcia_srom(si_t *sih, osl_t *osh, uint8 *pcmregs,
                              uint boff, uint16 *srom, uint bsz, bool check_crc);
#if defined(WLTEST) || defined(DHD_SPROM) || defined(BCMDBG)
static int set_si_pcmcia_srom(si_t *sih, osl_t *osh, uint8 *pcmregs,
                              uint boff, uint16 *srom, uint bsz);
#endif 
#endif 

#if defined(BCMUSBDEV_BMAC)
/* default to bcm94323 P200, other boards should have OTP programmed */
static char BCMATTACHDATA(defaultsromvars_4322usb)[] =
	"vendid=0x14e4\0"
	"subvendid=0x0a5c\0"
	"subdevid=0xbdc\0"
	"macaddr=00:90:4c:d3:04:73\0"
	"sromrev=8\0"
	"devid=0x432b\0"
	"boardrev=0x1200\0"
	"boardflags=0xa00\0"
	"boardflags2=0x602\0"
	"boardtype=0x04a8\0"
	"tssipos2g=0x1\0"
	"extpagain2g=0x0\0"
	"pdetrange2g=0x0\0"
	"triso2g=0x3\0"
	"antswctl2g=0x2\0"
	"tssipos5g=0x1\0"
	"extpagain5g=0x0\0"
	"pdetrange5g=0x0\0"
	"triso5g=0x3\0"
	"antswctl5g=0x2\0"
	"maxp2ga0=0x48\0"
	"itt2ga0=0x20\0"
	"pa2gw0a0=0xFEA8\0"
	"pa2gw1a0=0x16CD\0"
	"pa2gw2a0=0xFAA5\0"
	"maxp5ga0=0x40\0"
	"itt5ga0=0x3e\0"
	"maxp5gha0=0x3c\0"
	"maxp5gla0=0x40\0"
	"pa5gw0a0=0xFEB2\0"
	"pa5gw1a0=0x1471\0"
	"pa5gw2a0=0xFB1F\0"
	"pa5glw0a0=0xFEA2\0"
	"pa5glw1a0=0x149A\0"
	"pa5glw2a0=0xFAFC\0"
	"pa5ghw0a0=0xFEC6\0"
	"pa5ghw1a0=0x13DD\0"
	"pa5ghw2a0=0xFB48\0"
	"maxp2ga1=0x48\0"
	"itt2ga1=0x20\0"
	"pa2gw0a1=0xFEA3\0"
	"pa2gw1a1=0x1687\0"
	"pa2gw2a1=0xFAAA\0"
	"maxp5ga1=0x40\0"
	"itt5ga1=0x3e\0"
	"maxp5gha1=0x3c\0"
	"maxp5gla1=0x40\0"
	"pa5gw0a1=0xFEBC\0"
	"pa5gw1a1=0x14F9\0"
	"pa5gw2a1=0xFB05\0"
	"pa5glw0a1=0xFEBE\0"
	"pa5glw1a1=0x1478\0"
	"pa5glw2a1=0xFB1A\0"
	"pa5ghw0a1=0xFEE1\0"
	"pa5ghw1a1=0x14FD\0"
	"pa5ghw2a1=0xFB38\0"
	"cctl=0\0"
	"ccode=US\0"
	"regrev=0x0\0"
	"ledbh0=0xff\0"
	"ledbh1=0x2\0"
	"ledbh2=0x3\0"
	"ledbh3=0xff\0"
	"leddc=0xa0a0\0"
	"aa2g=0x3\0"
	"aa5g=0x3\0"
	"ag0=0x2\0"
	"ag1=0x2\0"
	"ag2=0xff\0"
	"ag3=0xff\0"
	"txchain=0x3\0"
	"rxchain=0x3\0"
	"antswitch=0\0"
	"END\0";

static char BCMATTACHDATA(defaultsromvars_43234usb)[] =
	"vendid=0x14e4\0"
	"subvendid=0x0a5c\0"
	"subdevid=0xbdc\0"
	"macaddr=00:90:4c:03:21:23\0"
	"cctl=0\0"
	"ccode=US\0"
	"regrev=0x0\0"
	"ledbh0=0x82\0"
	"ledbh1=0xff\0"
	"ledbh2=0xff\0"
	"ledbh3=0xff\0"
	"leddc=0x0\0"
	"aa2g=0x2\0"
	"aa5g=0x2\0"
	"ag0=0x2\0"
	"ag1=0x2\0"
	"ag2=0x2\0"
	"ag3=0xff\0"
	"txchain=0x2\0"
	"rxchain=0x2\0"
	"antswitch=0\0"
	"sromrev=8\0"
	"devid=0x4346\0"
	"boardrev=0x1113\0"
	"boardflags=0x200\0"
	"boardflags2=0x0\0"
	"boardtype=0x0521\0"
	"tssipos2g=0x1\0"
	"extpagain2g=0x2\0"
	"pdetrange2g=0x2\0"
	"triso2g=0x3\0"
	"antswctl2g=0x0\0"
	"tssipos5g=0x1\0"
	"extpagain5g=0x2\0"
	"pdetrange5g=0x2\0"
	"triso5g=0x3\0"
	"antswctl5g=0x0\0"
	"ofdm2gpo=0x44444444\0"
	"ofdm5gpo=0x0\0"
	"ofdm5glpo=0x0\0"
	"ofdm5ghpo=0x0\0"
	"mcs2gpo0=0x4444\0"
	"mcs2gpo1=0x4444\0"
	"mcs2gpo2=0x4444\0"
	"mcs2gpo3=0x4444\0"
	"mcs2gpo4=0x9999\0"
	"mcs2gpo5=0x9999\0"
	"mcs2gpo6=0x9999\0"
	"mcs2gpo7=0x9999\0"
	"mcs5gpo4=0x2222\0"
	"mcs5gpo5=0x2222\0"
	"mcs5gpo6=0x2222\0"
	"mcs5gpo7=0x2222\0"
	"mcs5glpo4=0x2222\0"
	"mcs5glpo5=0x2222\0"
	"mcs5glpo6=0x2222\0"
	"mcs5glpo7=0x2222\0"
	"mcs5ghpo4=0x2222\0"
	"mcs5ghpo5=0x2222\0"
	"mcs5ghpo6=0x2222\0"
	"mcs5ghpo7=0x2222\0"
	"maxp2ga0=0x4c\0"
	"itt2ga0=0x20\0"
	"itt5ga0=0x3e\0"
	"pa2gw0a0=0xFF15\0"
	"pa2gw1a0=0x135B\0"
	"pa2gw2a0=0xFB5C\0"
	"maxp5ga0=0x3c\0"
	"maxp5gha0=0x3a\0"
	"maxp5gla0=0x3c\0"
	"pa5gw0a0=0xFE6A\0"
	"pa5gw1a0=0x1430\0"
	"pa5gw2a0=0xFAEB\0"
	"pa5glw0a0=0xFE64\0"
	"pa5glw1a0=0x13F7\0"
	"pa5glw2a0=0xFAF6\0"
	"pa5ghw0a0=0xFE70\0"
	"pa5ghw1a0=0x14DE\0"
	"pa5ghw2a0=0xFAC7\0"
	"maxp2ga1=0x4c\0"
	"itt2ga1=0x20\0"
	"itt5ga1=0x3e\0"
	"pa2gw0a1=0xFF11\0"
	"pa2gw1a1=0x1324\0"
	"pa2gw2a1=0xFB5E\0"
	"maxp5ga1=0x3c\0"
	"maxp5gha1=0x3a\0"
	"maxp5gla1=0x3c\0"
	"pa5gw0a1=0xFE7D\0"
	"pa5gw1a1=0x1449\0"
	"pa5gw2a1=0xFAED\0"
	"pa5glw0a1=0xFE87\0"
	"pa5glw1a1=0x14BE\0"
	"pa5glw2a1=0xFAD6\0"
	"pa5ghw0a1=0xFE62\0"
	"pa5ghw1a1=0x137E\0"
	"pa5ghw2a1=0xFB12\0"
	"END\0";

static char BCMATTACHDATA(defaultsromvars_43235usb)[] =
	"vendid=0x14e4\0"
	"subvendid=0x0a5c\0"
	"subdevid=0xbdc\0"
	"macaddr=00:90:4c:05:30:01\0"
	"ccode=US\0"
	"regrev=0x0\0"
	"ledbh0=0x82\0"
	"ledbh1=0xff\0"
	"ledbh2=0xff\0"
	"ledbh3=0xff\0"
	"leddc=0x0\0"
	"aa2g=0x3\0"
	"ag0=0x2\0"
	"ag1=0x2\0"
	"ag2=0xff\0"
	"ag3=0xff\0"
	"txchain=0x3\0"
	"rxchain=0x3\0"
	"antswitch=0\0"
	"sromrev=8\0"
	"devid=0x4347\0"
	"boardrev=0x1113\0"
	"boardflags=0x200\0"
	"boardflags2=0x0\0"
	"boardtype=0x0571\0"
	"tssipos2g=0x1\0"
	"extpagain2g=0x2\0"
	"pdetrange2g=0x2\0"
	"triso2g=0x3\0"
	"antswctl2g=0x0\0"
	"antswctl5g=0x0\0"
	"ofdm2gpo=0x0\0"
	"mcs2gpo0=0x0\0"
	"mcs2gpo1=0x0\0"
	"mcs2gpo2=0x0\0"
	"mcs2gpo3=0x0\0"
	"mcs2gpo4=0x2222\0"
	"mcs2gpo5=0x2222\0"
	"mcs2gpo6=0x2222\0"
	"mcs2gpo7=0x4444\0"
	"maxp2ga0=0x42\0"
	"itt2ga0=0x20\0"
	"pa2gw0a0=0xFF00\0"
	"pa2gw1a0=0x143C\0"
	"pa2gw2a0=0xFB27\0"
	"maxp2ga1=0x42\0"
	"itt2ga1=0x20\0"
	"pa2gw0a1=0xFF22\0"
	"pa2gw1a1=0x142E\0"
	"pa2gw2a1=0xFB45\0"
	"tempthresh=120\0"
	"temps_period=5\0"
	"temp_hysteresis=5\0"
	"END\0";

static char BCMATTACHDATA(defaultsromvars_43236usb)[] =
	"vendid=0x14e4\0"
	"subvendid=0x0a5c\0"
	"subdevid=0xbdc\0"
	"macaddr=00:90:4c:03:21:23\0"
	"cctl=0\0"
	"ccode=US\0"
	"regrev=0x0\0"
	"ledbh0=0x82\0"
	"ledbh1=0xff\0"
	"ledbh2=0xff\0"
	"ledbh3=0xff\0"
	"leddc=0x0\0"
	"aa2g=0x3\0"
	"aa5g=0x3\0"
	"ag0=0x2\0"
	"ag1=0x2\0"
	"ag2=0x2\0"
	"ag3=0xff\0"
	"txchain=0x3\0"
	"rxchain=0x3\0"
	"antswitch=0\0"
	"sromrev=8\0"
	"devid=0x4346\0"
	"boardrev=0x1409\0"
	"boardflags=0x200\0"
	"boardflags2=0x2000\0"
	"boardtype=0x0521\0"
	"tssipos2g=0x1\0"
	"extpagain2g=0x2\0"
	"pdetrange2g=0x2\0"
	"triso2g=0x3\0"
	"antswctl2g=0x0\0"
	"tssipos5g=0x1\0"
	"extpagain5g=0x2\0"
	"pdetrange5g=0x2\0"
	"triso5g=0x3\0"
	"antswctl5g=0x0\0"
	"ofdm2gpo=0x0\0"
	"ofdm5gpo=0x0\0"
	"ofdm5glpo=0x0\0"
	"ofdm5ghpo=0x0\0"
	"mcs2gpo0=0x0\0"
	"mcs2gpo1=0x0\0"
	"mcs2gpo2=0x0\0"
	"mcs2gpo3=0x0\0"
	"mcs2gpo4=0x4444\0"
	"mcs2gpo5=0x4444\0"
	"mcs2gpo6=0x4444\0"
	"mcs2gpo7=0x4444\0"
	"mcs5gpo4=0x2222\0"
	"mcs5gpo5=0x2222\0"
	"mcs5gpo6=0x2222\0"
	"mcs5gpo7=0x2222\0"
	"mcs5glpo4=0x2222\0"
	"mcs5glpo5=0x2222\0"
	"mcs5glpo6=0x2222\0"
	"mcs5glpo7=0x2222\0"
	"mcs5ghpo4=0x2222\0"
	"mcs5ghpo5=0x2222\0"
	"mcs5ghpo6=0x2222\0"
	"mcs5ghpo7=0x2222\0"
	"maxp2ga0=0x42\0"
	"itt2ga0=0x20\0"
	"itt5ga0=0x3e\0"
	"pa2gw0a0=0xFF21\0"
	"pa2gw1a0=0x13B7\0"
	"pa2gw2a0=0xFB44\0"
	"maxp5ga0=0x3e\0"
	"maxp5gha0=0x3a\0"
	"maxp5gla0=0x3c\0"
	"pa5gw0a0=0xFEB2\0"
	"pa5gw1a0=0x1570\0"
	"pa5gw2a0=0xFAD6\0"
	"pa5glw0a0=0xFE64\0"
	"pa5glw1a0=0x13F7\0"
	"pa5glw2a0=0xFAF6\0"
	"pa5ghw0a0=0xFEAB\0"
	"pa5ghw1a0=0x15BB\0"
	"pa5ghw2a0=0xFAC6\0"
	"maxp2ga1=0x42\0"
	"itt2ga1=0x20\0"
	"itt5ga1=0x3e\0"
	"pa2gw0a1=0xFF17\0"
	"pa2gw1a1=0x13C4\0"
	"pa2gw2a1=0xFB3C\0"
	"maxp5ga1=0x3e\0"
	"maxp5gha1=0x3a\0"
	"maxp5gla1=0x3c\0"
	"pa5gw0a1=0xFE6F\0"
	"pa5gw1a1=0x13CC\0"
	"pa5gw2a1=0xFAF8\0"
	"pa5glw0a1=0xFE87\0"
	"pa5glw1a1=0x14BE\0"
	"pa5glw2a1=0xFAD6\0"
	"pa5ghw0a1=0xFE68\0"
	"pa5ghw1a1=0x13E9\0"
	"pa5ghw2a1=0xFAF6\0"
	"tempthresh=120\0"
	"temps_period=5\0"
	"temp_hysteresis=5\0"
	"END\0";

static char BCMATTACHDATA(defaultsromvars_4319usb)[] =
	"sromrev=3\0"
	"vendid=0x14e4\0"
	"devid=0x4338\0"
	"boardtype=0x4ee\0"
	"boardrev=0x1103\0"
	"boardflags=0x400201\0"
	"boardflags2=0x80\0"
	"xtalfreq=30000\0"
	"aa2g=1\0"
	"aa5g=0\0"
	"ag0=0\0"
	"opo=0\0"
	"pa0b0=0x1675\0"
	"pa0b1=0xfa74\0"
	"pa0b2=0xfea1\0"
	"pa0itssit=62\0"
	"pa0maxpwr=78\0"
	"rssismf2g=0xa\0"
	"rssismc2g=0xb\0"
	"rssisav2g=0x3\0"
	"bxa2g=0\0"
	"cckdigfilttype=6\0"
	"rxpo2g=2\0"
	"cckpo=0\0"
	"ofdmpo=0x22220000\0"
	"mcs2gpo0=0x6666\0"
	"mcs2gpo1=0x6666\0"
	"mcs2gpo2=0x0\0"
	"mcs2gpo3=0x0\0"
	"mcs2gpo4=0x6666\0"
	"mcs2gpo5=0x6666\0"
	"boardnum=291\0"
	"macaddr=00:90:4c:16:01:23\0"
	"END\0";
#endif  /* BCMUSBDEV_BMAC */


/* BCMHOSTVARS is enabled only if WLTEST is enabled or BCMEXTNVM is enabled */
#if defined(BCMHOSTVARS)
/* Also used by wl_readconfigdata for vars download */
char BCMATTACHDATA(mfgsromvars)[VARS_MAX];
int BCMATTACHDATA(defvarslen) = 0;
#endif 

/* BCMHOSTVARS is enabled only if WLTEST is enabled or BCMEXTNVM is enabled */
#if defined(BCMHOSTVARS)
static char BCMATTACHDATA(defaultsromvars_4331)[] =
	"sromrev=9\0"
	"boardrev=0x1104\0"
	"boardflags=0x200\0"
	"boardflags2=0x0\0"
	"boardtype=0x524\0"
	"boardvendor=0x14e4\0"
	"boardnum=0x2064\0"
	"macaddr=00:90:4c:1a:20:64\0"
	"ccode=0x0\0"
	"regrev=0x0\0"
	"ledbh0=0xff\0"
	"ledbh1=0xff\0"
	"ledbh2=0xff\0"
	"ledbh3=0xff\0"
	"leddc=0xffff\0"
	"opo=0x0\0"
	"aa2g=0x7\0"
	"aa5g=0x7\0"
	"ag0=0x2\0"
	"ag1=0x2\0"
	"ag2=0x2\0"
	"ag3=0xff\0"
	"pa0b0=0xfe7f\0"
	"pa0b1=0x15d9\0"
	"pa0b2=0xfac6\0"
	"pa0itssit=0x20\0"
	"pa0maxpwr=0x48\0"
	"pa1b0=0xfe89\0"
	"pa1b1=0x14b1\0"
	"pa1b2=0xfada\0"
	"pa1lob0=0xffff\0"
	"pa1lob1=0xffff\0"
	"pa1lob2=0xffff\0"
	"pa1hib0=0xfe8f\0"
	"pa1hib1=0x13df\0"
	"pa1hib2=0xfafa\0"
	"pa1itssit=0x3e\0"
	"pa1maxpwr=0x3c\0"
	"pa1lomaxpwr=0x3c\0"
	"pa1himaxpwr=0x3c\0"
	"bxa2g=0x3\0"
	"rssisav2g=0x7\0"
	"rssismc2g=0xf\0"
	"rssismf2g=0xf\0"
	"bxa5g=0x3\0"
	"rssisav5g=0x7\0"
	"rssismc5g=0xf\0"
	"rssismf5g=0xf\0"
	"tri2g=0xff\0"
	"tri5g=0xff\0"
	"tri5gl=0xff\0"
	"tri5gh=0xff\0"
	"rxpo2g=0xff\0"
	"rxpo5g=0xff\0"
	"txchain=0x7\0"
	"rxchain=0x7\0"
	"antswitch=0x0\0"
	"tssipos2g=0x1\0"
	"extpagain2g=0x2\0"
	"pdetrange2g=0x4\0"
	"triso2g=0x3\0"
	"antswctl2g=0x0\0"
	"tssipos5g=0x1\0"
	"elna2g=0xff\0"
	"extpagain5g=0x2\0"
	"pdetrange5g=0x4\0"
	"triso5g=0x3\0"
	"antswctl5g=0x0\0"
	"elna5g=0xff\0"
	"cckbw202gpo=0x0\0"
	"cckbw20ul2gpo=0x0\0"
	"legofdmbw202gpo=0x0\0"
	"legofdmbw20ul2gpo=0x0\0"
	"legofdmbw205glpo=0x0\0"
	"legofdmbw20ul5glpo=0x0\0"
	"legofdmbw205gmpo=0x0\0"
	"legofdmbw20ul5gmpo=0x0\0"
	"legofdmbw205ghpo=0x0\0"
	"legofdmbw20ul5ghpo=0x0\0"
	"mcsbw202gpo=0x0\0"
	"mcsbw20ul2gpo=0x0\0"
	"mcsbw402gpo=0x0\0"
	"mcsbw205glpo=0x0\0"
	"mcsbw20ul5glpo=0x0\0"
	"mcsbw405glpo=0x0\0"
	"mcsbw205gmpo=0x0\0"
	"mcsbw20ul5gmpo=0x0\0"
	"mcsbw405gmpo=0x0\0"
	"mcsbw205ghpo=0x0\0"
	"mcsbw20ul5ghpo=0x0\0"
	"mcsbw405ghpo=0x0\0"
	"mcs32po=0x0\0"
	"legofdm40duppo=0x0\0"
	"maxp2ga0=0x48\0"
	"itt2ga0=0x20\0"
	"itt5ga0=0x3e\0"
	"pa2gw0a0=0xfe7f\0"
	"pa2gw1a0=0x15d9\0"
	"pa2gw2a0=0xfac6\0"
	"maxp5ga0=0x3c\0"
	"maxp5gha0=0x3c\0"
	"maxp5gla0=0x3c\0"
	"pa5gw0a0=0xfe89\0"
	"pa5gw1a0=0x14b1\0"
	"pa5gw2a0=0xfada\0"
	"pa5glw0a0=0xffff\0"
	"pa5glw1a0=0xffff\0"
	"pa5glw2a0=0xffff\0"
	"pa5ghw0a0=0xfe8f\0"
	"pa5ghw1a0=0x13df\0"
	"pa5ghw2a0=0xfafa\0"
	"maxp2ga1=0x48\0"
	"itt2ga1=0x20\0"
	"itt5ga1=0x3e\0"
	"pa2gw0a1=0xfe54\0"
	"pa2gw1a1=0x1563\0"
	"pa2gw2a1=0xfa7f\0"
	"maxp5ga1=0x3c\0"
	"maxp5gha1=0x3c\0"
	"maxp5gla1=0x3c\0"
	"pa5gw0a1=0xfe53\0"
	"pa5gw1a1=0x14fe\0"
	"pa5gw2a1=0xfa94\0"
	"pa5glw0a1=0xffff\0"
	"pa5glw1a1=0xffff\0"
	"pa5glw2a1=0xffff\0"
	"pa5ghw0a1=0xfe6e\0"
	"pa5ghw1a1=0x1457\0"
	"pa5ghw2a1=0xfab9\0"
	"END\0";
#endif 

/* BCMHOSTVARS is enabled only if WLTEST is enabled or BCMEXTNVM is enabled */
#if defined(BCMHOSTVARS)
static char BCMATTACHDATA(defaultsromvars_wltest)[] =
	"macaddr=00:90:4c:f8:00:01\0"
	"et0macaddr=00:11:22:33:44:52\0"
	"et0phyaddr=30\0"
	"et0mdcport=0\0"
	"gpio2=robo_reset\0"
	"boardvendor=0x14e4\0"
	"boardflags=0x210\0"
	"boardflags2=0\0"
	"boardtype=0x04c3\0"
	"boardrev=0x1100\0"
	"sromrev=8\0"
	"devid=0x432c\0"
	"ccode=0\0"
	"regrev=0\0"
	"ledbh0=255\0"
	"ledbh1=255\0"
	"ledbh2=255\0"
	"ledbh3=255\0"
	"leddc=0xffff\0"
	"aa2g=3\0"
	"ag0=2\0"
	"ag1=2\0"
	"aa5g=3\0"
	"aa0=2\0"
	"aa1=2\0"
	"txchain=3\0"
	"rxchain=3\0"
	"antswitch=0\0"
	"itt2ga0=0x20\0"
	"maxp2ga0=0x48\0"
	"pa2gw0a0=0xfe9e\0"
	"pa2gw1a0=0x15d5\0"
	"pa2gw2a0=0xfae9\0"
	"itt2ga1=0x20\0"
	"maxp2ga1=0x48\0"
	"pa2gw0a1=0xfeb3\0"
	"pa2gw1a1=0x15c9\0"
	"pa2gw2a1=0xfaf7\0"
	"tssipos2g=1\0"
	"extpagain2g=0\0"
	"pdetrange2g=0\0"
	"triso2g=3\0"
	"antswctl2g=0\0"
	"tssipos5g=1\0"
	"extpagain5g=0\0"
	"pdetrange5g=0\0"
	"triso5g=3\0"
	"antswctl5g=0\0"
	"cck2gpo=0\0"
	"ofdm2gpo=0\0"
	"mcs2gpo0=0\0"
	"mcs2gpo1=0\0"
	"mcs2gpo2=0\0"
	"mcs2gpo3=0\0"
	"mcs2gpo4=0\0"
	"mcs2gpo5=0\0"
	"mcs2gpo6=0\0"
	"mcs2gpo7=0\0"
	"cddpo=0\0"
	"stbcpo=0\0"
	"bw40po=4\0"
	"bwduppo=0\0"
	"END\0";
#endif 

/* BCMHOSTVARS is enabled only if WLTEST is enabled or BCMEXTNVM is enabled */ 
#if defined(BCMHOSTVARS) || (defined(BCMUSBDEV_BMAC) || defined(BCM_BMAC_VARS_APPEND))
/* It must end with pattern of "END" */
static uint
BCMATTACHFN(srom_vars_len)(char *vars)
{
	uint pos = 0;
	uint len;
	char *s;

	for (s = vars; s && *s;) {

		if (strcmp(s, "END") == 0)
			break;

		len = strlen(s);
		s += strlen(s) + 1;
		pos += len + 1;
		/* BS_ERROR(("len %d vars[pos] %s\n", pos, s)); */
		if (pos > 4000) {
			return 0;
		}
	}

	return pos + 4;	/* include the "END\0" */
}
#endif 

/* Initialization of varbuf structure */
static void
BCMATTACHFN(varbuf_init)(varbuf_t *b, char *buf, uint size)
{
	b->size = size;
	b->base = b->buf = buf;
}

/* append a null terminated var=value string */
static int
BCMATTACHFN(varbuf_append)(varbuf_t *b, const char *fmt, ...)
{
	va_list ap;
	int r;
	size_t len;
	char *s;

	if (b->size < 2)
	  return 0;

	va_start(ap, fmt);
	r = vsnprintf(b->buf, b->size, fmt, ap);
	va_end(ap);

	/* C99 snprintf behavior returns r >= size on overflow,
	 * others return -1 on overflow.
	 * All return -1 on format error.
	 * We need to leave room for 2 null terminations, one for the current var
	 * string, and one for final null of the var table. So check that the
	 * strlen written, r, leaves room for 2 chars.
	 */
	if ((r == -1) || (r > (int)(b->size - 2))) {
		b->size = 0;
		return 0;
	}

	/* Remove any earlier occurrence of the same variable */
	if ((s = strchr(b->buf, '=')) != NULL) {
		len = (size_t)(s - b->buf);
		for (s = b->base; s < b->buf;) {
			if ((bcmp(s, b->buf, len) == 0) && s[len] == '=') {
				len = strlen(s) + 1;
				memmove(s, (s + len), ((b->buf + r + 1) - (s + len)));
				b->buf -= len;
				b->size += (unsigned int)len;
				break;
			}

			while (*s++)
				;
		}
	}

	/* skip over this string's null termination */
	r++;
	b->size -= r;
	b->buf += r;

	return r;
}

/*
 * Initialize local vars from the right source for this platform.
 * Return 0 on success, nonzero on error.
 */
int
BCMATTACHFN(srom_var_init)(si_t *sih, uint bustype, void *curmap, osl_t *osh,
	char **vars, uint *count)
{
	uint len;

	len = 0;

	ASSERT(bustype == BUSTYPE(bustype));
	if (vars == NULL || count == NULL)
		return (0);

	*vars = NULL;
	*count = 0;

	switch (BUSTYPE(bustype)) {
	case SI_BUS:
	case JTAG_BUS:
		return initvars_srom_si(sih, osh, curmap, vars, count);

	case PCI_BUS:
		ASSERT(curmap != NULL);
		if (curmap == NULL)
			return (-1);

		return initvars_srom_pci(sih, curmap, vars, count);

	case PCMCIA_BUS:
		return initvars_cis_pcmcia(sih, osh, vars, count);


#ifdef BCMSPI
	case SPI_BUS:
		return initvars_cis_spi(osh, vars, count);
#endif /* BCMSPI */

	default:
		ASSERT(0);
	}
	return (-1);
}

/* support only 16-bit word read from srom */
int
srom_read(si_t *sih, uint bustype, void *curmap, osl_t *osh,
          uint byteoff, uint nbytes, uint16 *buf, bool check_crc)
{
	uint i, off, nw;

	ASSERT(bustype == BUSTYPE(bustype));

	/* check input - 16-bit access only */
	if (byteoff & 1 || nbytes & 1 || (byteoff + nbytes) > SROM_MAX)
		return 1;

	off = byteoff / 2;
	nw = nbytes / 2;

	if (BUSTYPE(bustype) == PCI_BUS) {
		if (!curmap)
			return 1;

		if (si_is_sprom_available(sih)) {
			uint16 *srom;

			srom = (uint16 *)SROM_OFFSET(sih);
			if (srom == NULL)
				return 1;

			if (sprom_read_pci(osh, sih, srom, off, buf, nw, check_crc))
				return 1;
		}
#if defined(BCMNVRAMW) || defined(BCMNVRAMR)
		else {
			if (otp_read_pci(osh, sih, buf, SROM_MAX))
				return 1;
		}
#endif
	} else if (BUSTYPE(bustype) == PCMCIA_BUS) {
		for (i = 0; i < nw; i++) {
			if (sprom_read_pcmcia(osh, (uint16)(off + i), (uint16 *)(buf + i)))
				return 1;
		}
#ifdef BCMSPI
	} else if (BUSTYPE(bustype) == SPI_BUS) {
	                if (bcmsdh_cis_read(NULL, SDIO_FUNC_1, (uint8 *)buf, byteoff + nbytes) != 0)
				return 1;
#endif /* BCMSPI */
	} else if (BUSTYPE(bustype) == SI_BUS) {
#if defined(BCMUSBDEV)
		if (SPROMBUS == PCMCIA_BUS) {
			uint origidx;
			void *regs;
			int rc;
			bool wasup;

			/* Don't bother if we can't talk to SPROM */
			if (!si_is_sprom_available(sih))
				return 1;

			origidx = si_coreidx(sih);
			regs = si_setcore(sih, PCMCIA_CORE_ID, 0);
			if (!regs)
				regs = si_setcore(sih, SDIOD_CORE_ID, 0);
			ASSERT(regs != NULL);

			if (!(wasup = si_iscoreup(sih)))
				si_core_reset(sih, 0, 0);

			rc = get_si_pcmcia_srom(sih, osh, regs, byteoff, buf, nbytes, check_crc);

			if (!wasup)
				si_core_disable(sih, 0);

			si_setcoreidx(sih, origidx);
			return rc;
		}
#endif 

		return 1;
	} else {
		return 1;
	}

	return 0;
}

#if defined(WLTEST) || defined(DHD_SPROM) || defined(BCMDBG)
/* support only 16-bit word write into srom */
int
srom_write(si_t *sih, uint bustype, void *curmap, osl_t *osh,
           uint byteoff, uint nbytes, uint16 *buf)
{
	uint i, nw, crc_range;
	uint16 *old, *new;
	uint8 crc;
	volatile uint32 val32;
	int rc = 1;

	ASSERT(bustype == BUSTYPE(bustype));

	old = MALLOC(osh, SROM_MAXW * sizeof(uint16));
	new = MALLOC(osh, SROM_MAXW * sizeof(uint16));

	if (old == NULL || new == NULL)
		goto done;

	/* check input - 16-bit access only. use byteoff 0x55aa to indicate
	 * srclear
	 */
	if ((byteoff != 0x55aa) && ((byteoff & 1) || (nbytes & 1)))
		goto done;

	if ((byteoff != 0x55aa) && ((byteoff + nbytes) > SROM_MAX))
		goto done;

	if (BUSTYPE(bustype) == PCMCIA_BUS) {
		crc_range = SROM_MAX;
	}
#if defined(BCMUSBDEV)
	else {
		crc_range = srom_size(sih, osh);
	}
#else
	else {
		crc_range = (SROM8_SIGN + 1) * 2;	/* must big enough for SROM8 */
	}
#endif 

	nw = crc_range / 2;
	/* read first small number words from srom, then adjust the length, read all */
	if (srom_read(sih, bustype, curmap, osh, 0, crc_range, old, FALSE))
		goto done;

	BS_ERROR(("%s: old[SROM4_SIGN] 0x%x, old[SROM8_SIGN] 0x%x\n",
	          __FUNCTION__, old[SROM4_SIGN], old[SROM8_SIGN]));
	/* Deal with blank srom */
	if (old[0] == 0xffff) {
		/* see if the input buffer is valid SROM image or not */
		if ((buf[SROM4_SIGN] == SROM4_SIGNATURE) ||
			(buf[SROM8_SIGN] == SROM4_SIGNATURE)) {
			BS_ERROR(("%s: buf[SROM4_SIGN] 0x%x, buf[SROM8_SIGN] 0x%x\n",
				__FUNCTION__, buf[SROM4_SIGN], buf[SROM8_SIGN]));

			/* block invalid buffer size */
			if (nbytes < SROM4_WORDS * 2) {
				rc = BCME_BUFTOOSHORT;
				goto done;
			} else if (nbytes > SROM4_WORDS * 2) {
				rc = BCME_BUFTOOLONG;
				goto done;
			}

			nw = SROM4_WORDS;
		} else if (nbytes == SROM_WORDS * 2){ /* the other possible SROM format */
			BS_ERROR(("%s: Not SROM4 or SROM8.\n", __FUNCTION__));

			nw = SROM_WORDS;
		} else {
			BS_ERROR(("%s: Invalid input file signature\n", __FUNCTION__));
			rc = BCME_BADARG;
			goto done;
		}
		crc_range = nw * 2;
		if (srom_read(sih, bustype, curmap, osh, 0, crc_range, old, FALSE))
			goto done;
	} else if ((old[SROM4_SIGN] == SROM4_SIGNATURE) ||
	           (old[SROM8_SIGN] == SROM4_SIGNATURE)) {
		nw = SROM4_WORDS;
		crc_range = nw * 2;
		if (srom_read(sih, bustype, curmap, osh, 0, crc_range, old, FALSE))
			goto done;
	} else {
		/* Assert that we have already read enough for sromrev 2 */
		ASSERT(crc_range >= SROM_WORDS * 2);
		nw = SROM_WORDS;
		crc_range = nw * 2;
	}

	if (byteoff == 0x55aa) {
		/* Erase request */
		crc_range = 0;
		memset((void *)new, 0xff, nw * 2);
	} else {
		/* Copy old contents */
		bcopy((void *)old, (void *)new, nw * 2);
		/* make changes */
		bcopy((void *)buf, (void *)&new[byteoff / 2], nbytes);
	}

	if (crc_range) {
		/* calculate crc */
		htol16_buf(new, crc_range);
		crc = ~hndcrc8((uint8 *)new, crc_range - 1, CRC8_INIT_VALUE);
		ltoh16_buf(new, crc_range);
		new[nw - 1] = (crc << 8) | (new[nw - 1] & 0xff);
	}

	if (BUSTYPE(bustype) == PCI_BUS) {
		uint16 *srom = NULL;
		void *ccregs = NULL;
		uint32 ccval = 0;

		if ((CHIPID(sih->chip) == BCM4331_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM43431_CHIP_ID)) {
			/* save current control setting */
			ccval = si_chipcontrl_epa4331_read(sih);
			/* Disable Ext PA lines to allow reading from SROM */
			si_chipcontrl_epa4331(sih, FALSE);
		}

		/* enable writes to the SPROM */
		if (sih->ccrev > 31) {
			ccregs = (void *)((uint8 *)curmap + PCI_16KB0_CCREGS_OFFSET);
			srom = (uint16 *)((uint8 *)ccregs + CC_SROM_OTP);
			(void)srom_cc_cmd(sih, osh, ccregs, SRC_OP_WREN, 0, 0);
		} else {
			srom = (uint16 *)((uint8 *)curmap + PCI_BAR0_SPROM_OFFSET);
			val32 = OSL_PCI_READ_CONFIG(osh, PCI_SPROM_CONTROL, sizeof(uint32));
			val32 |= SPROM_WRITEEN;
			OSL_PCI_WRITE_CONFIG(osh, PCI_SPROM_CONTROL, sizeof(uint32), val32);
		}
		bcm_mdelay(WRITE_ENABLE_DELAY);
		/* write srom */
		for (i = 0; i < nw; i++) {
			if (old[i] != new[i]) {
				if (sih->ccrev > 31) {
					if ((sih->cccaps & CC_CAP_SROM) == 0) {
						/* No srom support in this chip */
						BS_ERROR(("srom_write, invalid srom, skip\n"));
					} else
						(void)srom_cc_cmd(sih, osh, ccregs, SRC_OP_WRITE,
							i, new[i]);
				} else {
					W_REG(osh, &srom[i], new[i]);
				}
				bcm_mdelay(WRITE_WORD_DELAY);
			}
		}
		/* disable writes to the SPROM */
		if (sih->ccrev > 31) {
			(void)srom_cc_cmd(sih, osh, ccregs, SRC_OP_WRDIS, 0, 0);
		} else {
			OSL_PCI_WRITE_CONFIG(osh, PCI_SPROM_CONTROL, sizeof(uint32), val32 &
			                     ~SPROM_WRITEEN);
		}

		if ((CHIPID(sih->chip) == BCM4331_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM43431_CHIP_ID)) {
			/* Restore config after reading SROM */
			si_chipcontrl_epa4331_restore(sih, ccval);
		}

	} else if (BUSTYPE(bustype) == PCMCIA_BUS) {
		/* enable writes to the SPROM */
		if (sprom_cmd_pcmcia(osh, SROM_WEN))
			goto done;
		bcm_mdelay(WRITE_ENABLE_DELAY);
		/* write srom */
		for (i = 0; i < nw; i++) {
			if (old[i] != new[i]) {
				sprom_write_pcmcia(osh, (uint16)(i), new[i]);
				bcm_mdelay(WRITE_WORD_DELAY);
			}
		}
		/* disable writes to the SPROM */
		if (sprom_cmd_pcmcia(osh, SROM_WDS))
			goto done;
	} else if (BUSTYPE(bustype) == SI_BUS) {
#if defined(BCMUSBDEV)
		if (SPROMBUS == PCMCIA_BUS) {
			uint origidx;
			void *regs;
			bool wasup;

			origidx = si_coreidx(sih);
			regs = si_setcore(sih, PCMCIA_CORE_ID, 0);
			if (!regs)
				regs = si_setcore(sih, SDIOD_CORE_ID, 0);
			ASSERT(regs != NULL);

			if (!(wasup = si_iscoreup(sih)))
				si_core_reset(sih, 0, 0);

			rc = set_si_pcmcia_srom(sih, osh, regs, byteoff, buf, nbytes);

			if (!wasup)
				si_core_disable(sih, 0);

			si_setcoreidx(sih, origidx);
			goto done;
		}
#endif 
		goto done;
	} else {
		goto done;
	}

	bcm_mdelay(WRITE_ENABLE_DELAY);
	rc = 0;

done:
	if (old != NULL)
		MFREE(osh, old, SROM_MAXW * sizeof(uint16));
	if (new != NULL)
		MFREE(osh, new, SROM_MAXW * sizeof(uint16));

	return rc;
}
#endif 

#if defined(BCMUSBDEV)
#define SI_PCMCIA_READ(osh, regs, fcr) \
		R_REG(osh, (volatile uint8 *)(regs) + 0x600 + (fcr) - 0x700 / 2)
#define SI_PCMCIA_WRITE(osh, regs, fcr, v) \
		W_REG(osh, (volatile uint8 *)(regs) + 0x600 + (fcr) - 0x700 / 2, v)

/* set PCMCIA srom command register */
static int
srom_cmd_si_pcmcia(osl_t *osh, uint8 *pcmregs, uint8 cmd)
{
	uint8 status = 0;
	uint wait_cnt = 0;

	/* write srom command register */
	SI_PCMCIA_WRITE(osh, pcmregs, SROM_CS, cmd);

	/* wait status */
	while (++wait_cnt < 1000000) {
		status = SI_PCMCIA_READ(osh, pcmregs, SROM_CS);
		if (status & SROM_DONE)
			return 0;
		OSL_DELAY(1);
	}

	BS_ERROR(("sr_cmd: Give up after %d tries, stat = 0x%x\n", wait_cnt, status));
	return 1;
}

/* read a word from the PCMCIA srom over SI */
static int
srom_read_si_pcmcia(osl_t *osh, uint8 *pcmregs, uint16 addr, uint16 *data)
{
	uint8 addr_l, addr_h,  data_l, data_h;

	addr_l = (uint8)((addr * 2) & 0xff);
	addr_h = (uint8)(((addr * 2) >> 8) & 0xff);

	/* set address */
	SI_PCMCIA_WRITE(osh, pcmregs, SROM_ADDRH, addr_h);
	SI_PCMCIA_WRITE(osh, pcmregs, SROM_ADDRL, addr_l);

	/* do read */
	if (srom_cmd_si_pcmcia(osh, pcmregs, SROM_READ))
		return 1;

	/* read data */
	data_h = SI_PCMCIA_READ(osh, pcmregs, SROM_DATAH);
	data_l = SI_PCMCIA_READ(osh, pcmregs, SROM_DATAL);
	*data = ((uint16)data_h << 8) | data_l;

	return 0;
}

#if defined(WLTEST) || defined(DHD_SPROM) || defined(BCMDBG)
/* write a word to the PCMCIA srom over SI */
static int
srom_write_si_pcmcia(osl_t *osh, uint8 *pcmregs, uint16 addr, uint16 data)
{
	uint8 addr_l, addr_h, data_l, data_h;
	int rc;

	addr_l = (uint8)((addr * 2) & 0xff);
	addr_h = (uint8)(((addr * 2) >> 8) & 0xff);

	/* set address */
	SI_PCMCIA_WRITE(osh, pcmregs, SROM_ADDRH, addr_h);
	SI_PCMCIA_WRITE(osh, pcmregs, SROM_ADDRL, addr_l);

	data_l = (uint8)(data & 0xff);
	data_h = (uint8)((data >> 8) & 0xff);

	/* write data */
	SI_PCMCIA_WRITE(osh, pcmregs, SROM_DATAH, data_h);
	SI_PCMCIA_WRITE(osh, pcmregs, SROM_DATAL, data_l);

	/* do write */
	rc = srom_cmd_si_pcmcia(osh, pcmregs, SROM_WRITE);
	OSL_DELAY(20000);
	return rc;
}
#endif 

/*
 * Read the srom for the pcmcia-srom over si case.
 * Return 0 on success, nonzero on error.
 */
static int
get_si_pcmcia_srom(si_t *sih, osl_t *osh, uint8 *pcmregs,
                   uint boff, uint16 *srom, uint bsz, bool check_crc)
{
	uint i, nw, woff, wsz;
	int err = 0;

	/* read must be at word boundary */
	ASSERT((boff & 1) == 0 && (bsz & 1) == 0);

	/* read sprom size and validate the parms */
	if ((nw = srom_size(sih, osh)) == 0) {
		BS_ERROR(("get_si_pcmcia_srom: sprom size unknown\n"));
		err = -1;
		goto out;
	}
	if (boff + bsz > 2 * nw) {
		BS_ERROR(("get_si_pcmcia_srom: sprom size exceeded\n"));
		err = -2;
		goto out;
	}

	/* read in sprom contents */
	for (woff = boff / 2, wsz = bsz / 2, i = 0;
	     woff < nw && i < wsz; woff ++, i ++) {
		if (srom_read_si_pcmcia(osh, pcmregs, (uint16)woff, &srom[i])) {
			BS_ERROR(("get_si_pcmcia_srom: sprom read failed\n"));
			err = -3;
			goto out;
		}
	}

	if (check_crc) {
		if (srom[0] == 0xffff) {
			/* The hardware thinks that an srom that starts with 0xffff
			 * is blank, regardless of the rest of the content, so declare
			 * it bad.
			 */
			BS_ERROR(("%s: srom[0] == 0xffff, assuming unprogrammed srom\n",
			          __FUNCTION__));
			err = -4;
			goto out;
		}

		/* fixup the endianness so crc8 will pass */
		htol16_buf(srom, nw * 2);
		if (hndcrc8((uint8 *)srom, nw * 2, CRC8_INIT_VALUE) != CRC8_GOOD_VALUE) {
			BS_ERROR(("%s: bad crc\n", __FUNCTION__));
			err = -5;
		}
		/* now correct the endianness of the byte array */
		ltoh16_buf(srom, nw * 2);
	}

out:
	return err;
}

#if defined(WLTEST) || defined(DHD_SPROM) || defined(BCMDBG)
/*
 * Write the srom for the pcmcia-srom over si case.
 * Return 0 on success, nonzero on error.
 */
static int
set_si_pcmcia_srom(si_t *sih, osl_t *osh, uint8 *pcmregs,
                   uint boff, uint16 *srom, uint bsz)
{
	uint i, nw, woff, wsz;
	uint16 word;
	uint8 crc;
	int err = 0;

	/* write must be at word boundary */
	ASSERT((boff & 1) == 0 && (bsz & 1) == 0);

	/* read sprom size and validate the parms */
	if ((nw = srom_size(sih, osh)) == 0) {
		BS_ERROR(("set_si_pcmcia_srom: sprom size unknown\n"));
		err = -1;
		goto out;
	}
	if (boff + bsz > 2 * nw) {
		BS_ERROR(("set_si_pcmcia_srom: sprom size exceeded\n"));
		err = -2;
		goto out;
	}

	/* enable write */
	if (srom_cmd_si_pcmcia(osh, pcmregs, SROM_WEN)) {
		BS_ERROR(("set_si_pcmcia_srom: sprom wen failed\n"));
		err = -3;
		goto out;
	}

	/* write buffer to sprom */
	for (woff = boff / 2, wsz = bsz / 2, i = 0;
	     woff < nw && i < wsz; woff ++, i ++) {
		if (srom_write_si_pcmcia(osh, pcmregs, (uint16)woff, srom[i])) {
			BS_ERROR(("set_si_pcmcia_srom: sprom write failed\n"));
			err = -4;
			goto out;
		}
	}

	/* fix crc */
	crc = CRC8_INIT_VALUE;
	for (woff = 0; woff < nw; woff ++) {
		if (srom_read_si_pcmcia(osh, pcmregs, (uint16)woff, &word)) {
			BS_ERROR(("set_si_pcmcia_srom: sprom fix crc read failed\n"));
			err = -5;
			goto out;
		}
		word = htol16(word);
		crc = hndcrc8((uint8 *)&word, woff != nw - 1 ? 2 : 1, crc);
	}
	word = (~crc << 8) + (ltoh16(word) & 0xff);
	if (srom_write_si_pcmcia(osh, pcmregs, (uint16)(woff - 1), word)) {
		BS_ERROR(("set_si_pcmcia_srom: sprom fix crc write failed\n"));
		err = -6;
		goto out;
	}

	/* disable write */
	if (srom_cmd_si_pcmcia(osh, pcmregs, SROM_WDS)) {
		BS_ERROR(("set_si_pcmcia_srom: sprom wds failed\n"));
		err = -7;
		goto out;
	}

out:
	return err;
}
#endif 
#endif 

static const char BCMATTACHDATA(vstr_manf)[] = "manf=%s";
static const char BCMATTACHDATA(vstr_productname)[] = "productname=%s";
static const char BCMATTACHDATA(vstr_manfid)[] = "manfid=0x%x";
static const char BCMATTACHDATA(vstr_prodid)[] = "prodid=0x%x";
static const char BCMATTACHDATA(vstr_regwindowsz)[] = "regwindowsz=%d";
static const char BCMATTACHDATA(vstr_sromrev)[] = "sromrev=%d";
static const char BCMATTACHDATA(vstr_chiprev)[] = "chiprev=%d";
static const char BCMATTACHDATA(vstr_subvendid)[] = "subvendid=0x%x";
static const char BCMATTACHDATA(vstr_subdevid)[] = "subdevid=0x%x";
static const char BCMATTACHDATA(vstr_boardrev)[] = "boardrev=0x%x";
static const char BCMATTACHDATA(vstr_aa2g)[] = "aa2g=0x%x";
static const char BCMATTACHDATA(vstr_aa5g)[] = "aa5g=0x%x";
static const char BCMATTACHDATA(vstr_ag)[] = "ag%d=0x%x";
static const char BCMATTACHDATA(vstr_cc)[] = "cc=%d";
static const char BCMATTACHDATA(vstr_opo)[] = "opo=%d";
static const char BCMATTACHDATA(vstr_pa0b)[][9] = { "pa0b0=%d", "pa0b1=%d", "pa0b2=%d" };
static const char BCMATTACHDATA(vstr_pa0itssit)[] = "pa0itssit=%d";
static const char BCMATTACHDATA(vstr_pa0maxpwr)[] = "pa0maxpwr=%d";
static const char BCMATTACHDATA(vstr_pa1b)[][9] = { "pa1b0=%d", "pa1b1=%d", "pa1b2=%d" };
static const char BCMATTACHDATA(vstr_pa1lob)[][11] =
	{ "pa1lob0=%d", "pa1lob1=%d", "pa1lob2=%d" };
static const char BCMATTACHDATA(vstr_pa1hib)[][11] =
	{ "pa1hib0=%d", "pa1hib1=%d", "pa1hib2=%d" };
static const char BCMATTACHDATA(vstr_pa1itssit)[] = "pa1itssit=%d";
static const char BCMATTACHDATA(vstr_pa1maxpwr)[] = "pa1maxpwr=%d";
static const char BCMATTACHDATA(vstr_pa1lomaxpwr)[] = "pa1lomaxpwr=%d";
static const char BCMATTACHDATA(vstr_pa1himaxpwr)[] = "pa1himaxpwr=%d";
static const char BCMATTACHDATA(vstr_oem)[] = "oem=%02x%02x%02x%02x%02x%02x%02x%02x";
static const char BCMATTACHDATA(vstr_boardflags)[] = "boardflags=0x%x";
static const char BCMATTACHDATA(vstr_boardflags2)[] = "boardflags2=0x%x";
static const char BCMATTACHDATA(vstr_ledbh)[] = "ledbh%d=0x%x";
static const char BCMATTACHDATA(vstr_noccode)[] = "ccode=0x0";
static const char BCMATTACHDATA(vstr_ccode)[] = "ccode=%c%c";
static const char BCMATTACHDATA(vstr_cctl)[] = "cctl=0x%x";
static const char BCMATTACHDATA(vstr_cckpo)[] = "cckpo=0x%x";
static const char BCMATTACHDATA(vstr_ofdmpo)[] = "ofdmpo=0x%x";
static const char BCMATTACHDATA(vstr_rdlid)[] = "rdlid=0x%x";
static const char BCMATTACHDATA(vstr_rdlrndis)[] = "rdlrndis=%d";
static const char BCMATTACHDATA(vstr_rdlrwu)[] = "rdlrwu=%d";
static const char BCMATTACHDATA(vstr_usbfs)[] = "usbfs=%d";
static const char BCMATTACHDATA(vstr_wpsgpio)[] = "wpsgpio=%d";
static const char BCMATTACHDATA(vstr_wpsled)[] = "wpsled=%d";
static const char BCMATTACHDATA(vstr_rdlsn)[] = "rdlsn=%d";
static const char BCMATTACHDATA(vstr_rssismf2g)[] = "rssismf2g=%d";
static const char BCMATTACHDATA(vstr_rssismc2g)[] = "rssismc2g=%d";
static const char BCMATTACHDATA(vstr_rssisav2g)[] = "rssisav2g=%d";
static const char BCMATTACHDATA(vstr_bxa2g)[] = "bxa2g=%d";
static const char BCMATTACHDATA(vstr_rssismf5g)[] = "rssismf5g=%d";
static const char BCMATTACHDATA(vstr_rssismc5g)[] = "rssismc5g=%d";
static const char BCMATTACHDATA(vstr_rssisav5g)[] = "rssisav5g=%d";
static const char BCMATTACHDATA(vstr_bxa5g)[] = "bxa5g=%d";
static const char BCMATTACHDATA(vstr_tri2g)[] = "tri2g=%d";
static const char BCMATTACHDATA(vstr_tri5gl)[] = "tri5gl=%d";
static const char BCMATTACHDATA(vstr_tri5g)[] = "tri5g=%d";
static const char BCMATTACHDATA(vstr_tri5gh)[] = "tri5gh=%d";
static const char BCMATTACHDATA(vstr_rxpo2g)[] = "rxpo2g=%d";
static const char BCMATTACHDATA(vstr_rxpo5g)[] = "rxpo5g=%d";
static const char BCMATTACHDATA(vstr_boardtype)[] = "boardtype=0x%x";
static const char BCMATTACHDATA(vstr_leddc)[] = "leddc=0x%04x";
static const char BCMATTACHDATA(vstr_vendid)[] = "vendid=0x%x";
static const char BCMATTACHDATA(vstr_devid)[] = "devid=0x%x";
static const char BCMATTACHDATA(vstr_xtalfreq)[] = "xtalfreq=%d";
static const char BCMATTACHDATA(vstr_txchain)[] = "txchain=0x%x";
static const char BCMATTACHDATA(vstr_rxchain)[] = "rxchain=0x%x";
static const char BCMATTACHDATA(vstr_antswitch)[] = "antswitch=0x%x";
static const char BCMATTACHDATA(vstr_regrev)[] = "regrev=0x%x";
static const char BCMATTACHDATA(vstr_antswctl2g)[] = "antswctl2g=0x%x";
static const char BCMATTACHDATA(vstr_triso2g)[] = "triso2g=0x%x";
static const char BCMATTACHDATA(vstr_pdetrange2g)[] = "pdetrange2g=0x%x";
static const char BCMATTACHDATA(vstr_extpagain2g)[] = "extpagain2g=0x%x";
static const char BCMATTACHDATA(vstr_tssipos2g)[] = "tssipos2g=0x%x";
static const char BCMATTACHDATA(vstr_antswctl5g)[] = "antswctl5g=0x%x";
static const char BCMATTACHDATA(vstr_triso5g)[] = "triso5g=0x%x";
static const char BCMATTACHDATA(vstr_pdetrange5g)[] = "pdetrange5g=0x%x";
static const char BCMATTACHDATA(vstr_extpagain5g)[] = "extpagain5g=0x%x";
static const char BCMATTACHDATA(vstr_tssipos5g)[] = "tssipos5g=0x%x";
static const char BCMATTACHDATA(vstr_maxp2ga0)[] = "maxp2ga0=0x%x";
static const char BCMATTACHDATA(vstr_itt2ga0)[] = "itt2ga0=0x%x";
static const char BCMATTACHDATA(vstr_pa)[] = "pa%dgw%da%d=0x%x";
static const char BCMATTACHDATA(vstr_pahl)[] = "pa%dg%cw%da%d=0x%x";
static const char BCMATTACHDATA(vstr_maxp5ga0)[] = "maxp5ga0=0x%x";
static const char BCMATTACHDATA(vstr_itt5ga0)[] = "itt5ga0=0x%x";
static const char BCMATTACHDATA(vstr_maxp5gha0)[] = "maxp5gha0=0x%x";
static const char BCMATTACHDATA(vstr_maxp5gla0)[] = "maxp5gla0=0x%x";
static const char BCMATTACHDATA(vstr_maxp2ga1)[] = "maxp2ga1=0x%x";
static const char BCMATTACHDATA(vstr_itt2ga1)[] = "itt2ga1=0x%x";
static const char BCMATTACHDATA(vstr_maxp5ga1)[] = "maxp5ga1=0x%x";
static const char BCMATTACHDATA(vstr_itt5ga1)[] = "itt5ga1=0x%x";
static const char BCMATTACHDATA(vstr_maxp5gha1)[] = "maxp5gha1=0x%x";
static const char BCMATTACHDATA(vstr_maxp5gla1)[] = "maxp5gla1=0x%x";
static const char BCMATTACHDATA(vstr_cck2gpo)[] = "cck2gpo=0x%x";
static const char BCMATTACHDATA(vstr_ofdm2gpo)[] = "ofdm2gpo=0x%x";
static const char BCMATTACHDATA(vstr_ofdm5gpo)[] = "ofdm5gpo=0x%x";
static const char BCMATTACHDATA(vstr_ofdm5glpo)[] = "ofdm5glpo=0x%x";
static const char BCMATTACHDATA(vstr_ofdm5ghpo)[] = "ofdm5ghpo=0x%x";
static const char BCMATTACHDATA(vstr_cddpo)[] = "cddpo=0x%x";
static const char BCMATTACHDATA(vstr_stbcpo)[] = "stbcpo=0x%x";
static const char BCMATTACHDATA(vstr_bw40po)[] = "bw40po=0x%x";
static const char BCMATTACHDATA(vstr_bwduppo)[] = "bwduppo=0x%x";
static const char BCMATTACHDATA(vstr_mcspo)[] = "mcs%dgpo%d=0x%x";
static const char BCMATTACHDATA(vstr_mcspohl)[] = "mcs%dg%cpo%d=0x%x";
static const char BCMATTACHDATA(vstr_custom)[] = "customvar%d=0x%x";
static const char BCMATTACHDATA(vstr_cckdigfilttype)[] = "cckdigfilttype=%d";
#ifdef BCM_BOOTLOADER
static const char BCMATTACHDATA(vstr_brmin)[] = "brmin=0x%x";
static const char BCMATTACHDATA(vstr_brmax)[] = "brmax=0x%x";
#endif /* BCM_BOOTLOADER */
static const char BCMATTACHDATA(vstr_boardnum)[] = "boardnum=%d";
static const char BCMATTACHDATA(vstr_macaddr)[] = "macaddr=%s";
static const char BCMATTACHDATA(vstr_usbepnum)[] = "usbepnum=0x%x";

/* Power per rate for SROM V9 */
static const char BCMATTACHDATA(vstr_cckbw202gpo)[][19] =
	{ "cckbw202gpo=0x%x", "cckbw20ul2gpo=0x%x" };
static const char BCMATTACHDATA(vstr_legofdmbw202gpo)[][22] =
	{ "legofdmbw202gpo=0x%x", "legofdmbw20ul2gpo=0x%x" };
static const char BCMATTACHDATA(vstr_legofdmbw205gpo)[][24] =
	{ "legofdmbw205glpo=0x%x", "legofdmbw20ul5glpo=0x%x",
	"legofdmbw205gmpo=0x%x", "legofdmbw20ul5gmpo=0x%x",
	"legofdmbw205ghpo=0x%x", "legofdmbw20ul5ghpo=0x%x" };

static const char BCMATTACHDATA(vstr_mcs2gpo)[][19] =
{ "mcsbw202gpo=0x%x", "mcsbw20ul2gpo=0x%x", "mcsbw402gpo=0x%x"};

static const char BCMATTACHDATA(vstr_mcs5glpo)[][20] =
	{ "mcsbw205glpo=0x%x", "mcsbw20ul5glpo=0x%x", "mcsbw405glpo=0x%x"};

static const char BCMATTACHDATA(vstr_mcs5gmpo)[][20] =
	{ "mcsbw205gmpo=0x%x", "mcsbw20ul5gmpo=0x%x", "mcsbw405gmpo=0x%x"};

static const char BCMATTACHDATA(vstr_mcs5ghpo)[][20] =
	{ "mcsbw205ghpo=0x%x", "mcsbw20ul5ghpo=0x%x", "mcsbw405ghpo=0x%x"};

static const char BCMATTACHDATA(vstr_mcs32po)[] = "mcs32po=0x%x";
static const char BCMATTACHDATA(vstr_legofdm40duppo)[] = "legofdm40duppo=0x%x";

static const char BCMATTACHDATA(vstr_tempthresh)[] = "tempthresh=%d";
static const char BCMATTACHDATA(vstr_temps_period)[] = "temps_period=%d";
static const char BCMATTACHDATA(vstr_temp_hysteresis)[] = "temp_hysteresis=%d";

static const char BCMATTACHDATA(vstr_uuid)[] = "uuid=%s";

static const char BCMATTACHDATA(vstr_end)[] = "END\0";

uint8 patch_pair = 0;

/* For dongle HW, accept partial calibration parameters */
#if defined(BCMUSBDEV)
#define BCMDONGLECASE(n) case n:
#else
#define BCMDONGLECASE(n)
#endif

int
BCMATTACHFN(srom_parsecis)(osl_t *osh, uint8 *pcis[], uint ciscnt, char **vars, uint *count)
{
	char eabuf[32];
	char *base;
	varbuf_t b;
	uint8 *cis, tup, tlen, sromrev = 1;
	int i, j;
#ifndef BCM_BOOTLOADER
	bool ag_init = FALSE;
#endif
	uint32 w32;
	uint funcid;
	uint cisnum;
	int32 boardnum;
	int err;
	bool standard_cis;

	ASSERT(vars != NULL);
	ASSERT(count != NULL);

	boardnum = -1;

	base = MALLOC(osh, MAXSZ_NVRAM_VARS);
	ASSERT(base != NULL);
	if (!base)
		return -2;

	varbuf_init(&b, base, MAXSZ_NVRAM_VARS);
	bzero(base, MAXSZ_NVRAM_VARS);
#ifdef BCM_BMAC_VARS_APPEND
	/* 43236 use defaultsromvars_43236usb as the base, 
	 * then append and update it with the content from OTP.
	 * Only revision/board specfic content or updates used to override 
	 * the driver default will be stored in OTP		
	 */ 
	*count -= (strlen(vstr_end) + 1 + 1); /* back off the termnating END\0\0 from fakenvram */
	bcopy(*vars, base, *count);
	b.buf += *count;
#endif /* BCM_BMAC_VARS_APPEND */
	eabuf[0] = '\0';
	for (cisnum = 0; cisnum < ciscnt; cisnum++) {
		cis = *pcis++;
		i = 0;
		funcid = 0;
		standard_cis = TRUE;
		do {
			if (standard_cis) {
				tup = cis[i++];
				if (tup == CISTPL_NULL || tup == CISTPL_END)
					tlen = 0;
				else
					tlen = cis[i++];
			} else {
				if (cis[i] == CISTPL_NULL || cis[i] == CISTPL_END) {
					tlen = 0;
					tup = cis[i];
				} else {
					tlen = cis[i];
					tup = CISTPL_BRCM_HNBU;
				}
				++i;
			}
			if ((i + tlen) >= CIS_SIZE)
				break;

			switch (tup) {
			case CISTPL_VERS_1:
				/* assume the strings are good if the version field checks out */
				if (((cis[i + 1] << 8) + cis[i]) >= 0x0008) {
					varbuf_append(&b, vstr_manf, &cis[i + 2]);
					varbuf_append(&b, vstr_productname,
					              &cis[i + 3 + strlen((char *)&cis[i + 2])]);
					break;
				}

			case CISTPL_MANFID:
				varbuf_append(&b, vstr_manfid, (cis[i + 1] << 8) + cis[i]);
				varbuf_append(&b, vstr_prodid, (cis[i + 3] << 8) + cis[i + 2]);
				break;

			case CISTPL_FUNCID:
				funcid = cis[i];
				break;

			case CISTPL_FUNCE:
				switch (funcid) {
				case CISTPL_FID_SDIO:
					funcid = 0;
					break;
				default:
					/* set macaddr if HNBU_MACADDR not seen yet */
					if (eabuf[0] == '\0' && cis[i] == LAN_NID &&
						!(ETHER_ISNULLADDR(&cis[i + 2])) &&
						!(ETHER_ISMULTI(&cis[i + 2]))) {
						ASSERT(cis[i + 1] == ETHER_ADDR_LEN);
						bcm_ether_ntoa((struct ether_addr *)&cis[i + 2],
						               eabuf);

						/* set boardnum if HNBU_BOARDNUM not seen yet */
						if (boardnum == -1)
							boardnum = (cis[i + 6] << 8) + cis[i + 7];
					}
					break;
				}
				break;

			case CISTPL_CFTABLE:
				varbuf_append(&b, vstr_regwindowsz, (cis[i + 7] << 8) | cis[i + 6]);
				break;

			case CISTPL_BRCM_HNBU:
				switch (cis[i]) {
				case HNBU_SROMREV:
					sromrev = cis[i + 1];
					varbuf_append(&b, vstr_sromrev, sromrev);
					break;

				case HNBU_XTALFREQ:
					varbuf_append(&b, vstr_xtalfreq,
					              (cis[i + 4] << 24) |
					              (cis[i + 3] << 16) |
					              (cis[i + 2] << 8) |
					              cis[i + 1]);
					break;

				case HNBU_CHIPID:
					varbuf_append(&b, vstr_vendid, (cis[i + 2] << 8) +
					              cis[i + 1]);
					varbuf_append(&b, vstr_devid, (cis[i + 4] << 8) +
					              cis[i + 3]);
					if (tlen >= 7) {
						varbuf_append(&b, vstr_chiprev,
						              (cis[i + 6] << 8) + cis[i + 5]);
					}
					if (tlen >= 9) {
						varbuf_append(&b, vstr_subvendid,
						              (cis[i + 8] << 8) + cis[i + 7]);
					}
					if (tlen >= 11) {
						varbuf_append(&b, vstr_subdevid,
						              (cis[i + 10] << 8) + cis[i + 9]);
						/* subdevid doubles for boardtype */
						varbuf_append(&b, vstr_boardtype,
						              (cis[i + 10] << 8) + cis[i + 9]);
					}
					break;

				case HNBU_BOARDNUM:
					boardnum = (cis[i + 2] << 8) + cis[i + 1];
					break;

				case HNBU_PATCH:
					{
						char vstr_paddr[16];
						char vstr_pdata[16];

						/* retrieve the patch pairs
						 * from tlen/6; where 6 is
						 * sizeof(patch addr(2)) +
						 * sizeof(patch data(4)).
						 */
						patch_pair = tlen/6;

						for (j = 0; j < patch_pair; j++) {
							snprintf(vstr_paddr, sizeof(vstr_paddr),
								"pa%d=0x%%x", j);
							snprintf(vstr_pdata, sizeof(vstr_pdata),
								"pd%d=0x%%x", j);

							varbuf_append(&b, vstr_paddr,
								(cis[i + (j*6) + 2] << 8) |
								cis[i + (j*6) + 1]);

							varbuf_append(&b, vstr_pdata,
								(cis[i + (j*6) + 6] << 24) |
								(cis[i + (j*6) + 5] << 16) |
								(cis[i + (j*6) + 4] << 8) |
								cis[i + (j*6) + 3]);
						}
					}
					break;

				case HNBU_BOARDREV:
					if (tlen == 2)
						varbuf_append(&b, vstr_boardrev, cis[i + 1]);
					else
						varbuf_append(&b, vstr_boardrev,
							(cis[i + 2] << 8) + cis[i + 1]);
					break;

				case HNBU_BOARDFLAGS:
					w32 = (cis[i + 2] << 8) + cis[i + 1];
					if (tlen >= 5)
						w32 |= ((cis[i + 4] << 24) + (cis[i + 3] << 16));
					varbuf_append(&b, vstr_boardflags, w32);

					if (tlen >= 7) {
						w32 = (cis[i + 6] << 8) + cis[i + 5];
						if (tlen >= 9)
							w32 |= ((cis[i + 8] << 24) +
								(cis[i + 7] << 16));
						varbuf_append(&b, vstr_boardflags2, w32);
					}
					break;

				case HNBU_USBFS:
					varbuf_append(&b, vstr_usbfs, cis[i + 1]);
					break;

				case HNBU_BOARDTYPE:
					varbuf_append(&b, vstr_boardtype,
					              (cis[i + 2] << 8) + cis[i + 1]);
					break;

				case HNBU_HNBUCIS:
					/*
					 * what follows is a nonstandard HNBU CIS
					 * that lacks CISTPL_BRCM_HNBU tags
					 *
					 * skip 0xff (end of standard CIS)
					 * after this tuple
					 */
					tlen++;
					standard_cis = FALSE;
					break;

				case HNBU_USBEPNUM:
					varbuf_append(&b, vstr_usbepnum,
						(cis[i + 2] << 8) | cis[i + 1]);
					break;

#ifdef BCM_BOOTLOADER
				case HNBU_BRMIN:
					varbuf_append(&b, vstr_brmin,
					              (cis[i + 4] << 24) |
					              (cis[i + 3] << 16) |
					              (cis[i + 2] << 8) |
					              cis[i + 1]);
					break;

				case HNBU_BRMAX:
					varbuf_append(&b, vstr_brmax,
					              (cis[i + 4] << 24) |
					              (cis[i + 3] << 16) |
					              (cis[i + 2] << 8) |
					              cis[i + 1]);
					break;
#endif /* BCM_BOOTLOADER */				

				case HNBU_RDLID:
					varbuf_append(&b, vstr_rdlid,
					              (cis[i + 2] << 8) | cis[i + 1]);
					break;

#ifdef BCM_BOOTLOADER
				case HNBU_RDLRNDIS:
					varbuf_append(&b, vstr_rdlrndis, cis[i + 1]);
					break;

				case HNBU_RDLRWU:
					varbuf_append(&b, vstr_rdlrwu, cis[i + 1]);
					break;

				case HNBU_RDLSN:
					if (tlen >= 5)
						varbuf_append(&b, vstr_rdlsn,
						              (cis[i + 4] << 24) |
						              (cis[i + 3] << 16) |
						              (cis[i + 2] << 8) |
						              cis[i + 1]);
					else
						varbuf_append(&b, vstr_rdlsn,
						              (cis[i + 2] << 8) |
						              cis[i + 1]);
					break;

#else
				case HNBU_AA:
					varbuf_append(&b, vstr_aa2g, cis[i + 1]);
					if (tlen >= 3)
						varbuf_append(&b, vstr_aa5g, cis[i + 2]);
					break;

				case HNBU_AG:
					varbuf_append(&b, vstr_ag, 0, cis[i + 1]);
					if (tlen >= 3)
						varbuf_append(&b, vstr_ag, 1, cis[i + 2]);
					if (tlen >= 4)
						varbuf_append(&b, vstr_ag, 2, cis[i + 3]);
					if (tlen >= 5)
						varbuf_append(&b, vstr_ag, 3, cis[i + 4]);
					ag_init = TRUE;
					break;

				case HNBU_ANT5G:
					varbuf_append(&b, vstr_aa5g, cis[i + 1]);
					varbuf_append(&b, vstr_ag, 1, cis[i + 2]);
					break;

				case HNBU_CC:
					ASSERT(sromrev == 1);
					varbuf_append(&b, vstr_cc, cis[i + 1]);
					break;

				case HNBU_PAPARMS:
					switch (tlen) {
					case 2:
						ASSERT(sromrev == 1);
						varbuf_append(&b, vstr_pa0maxpwr, cis[i + 1]);
						break;
					case 10:
						ASSERT(sromrev >= 2);
						varbuf_append(&b, vstr_opo, cis[i + 9]);
						/* FALLTHROUGH */
					case 9:
						varbuf_append(&b, vstr_pa0maxpwr, cis[i + 8]);
						/* FALLTHROUGH */
					BCMDONGLECASE(8)
						varbuf_append(&b, vstr_pa0itssit, cis[i + 7]);
						/* FALLTHROUGH */
					BCMDONGLECASE(7)
					        for (j = 0; j < 3; j++) {
							varbuf_append(&b, vstr_pa0b[j],
							              (cis[i + (j * 2) + 2] << 8) +
							              cis[i + (j * 2) + 1]);
						}
						break;
					default:
						ASSERT((tlen == 2) || (tlen == 9) || (tlen == 10));
						break;
					}
					break;

				case HNBU_PAPARMS5G:
					ASSERT((sromrev == 2) || (sromrev == 3));
					switch (tlen) {
					case 23:
						varbuf_append(&b, vstr_pa1himaxpwr, cis[i + 22]);
						varbuf_append(&b, vstr_pa1lomaxpwr, cis[i + 21]);
						varbuf_append(&b, vstr_pa1maxpwr, cis[i + 20]);
						/* FALLTHROUGH */
					case 20:
						varbuf_append(&b, vstr_pa1itssit, cis[i + 19]);
						/* FALLTHROUGH */
					case 19:
						for (j = 0; j < 3; j++) {
							varbuf_append(&b, vstr_pa1b[j],
							              (cis[i + (j * 2) + 2] << 8) +
							              cis[i + (j * 2) + 1]);
						}
						for (j = 3; j < 6; j++) {
							varbuf_append(&b, vstr_pa1lob[j - 3],
							              (cis[i + (j * 2) + 2] << 8) +
							              cis[i + (j * 2) + 1]);
						}
						for (j = 6; j < 9; j++) {
							varbuf_append(&b, vstr_pa1hib[j - 6],
							              (cis[i + (j * 2) + 2] << 8) +
							              cis[i + (j * 2) + 1]);
						}
						break;
					default:
						ASSERT((tlen == 19) ||
						       (tlen == 20) || (tlen == 23));
						break;
					}
					break;

				case HNBU_OEM:
					ASSERT(sromrev == 1);
					varbuf_append(&b, vstr_oem,
					              cis[i + 1], cis[i + 2],
					              cis[i + 3], cis[i + 4],
					              cis[i + 5], cis[i + 6],
					              cis[i + 7], cis[i + 8]);
					break;

				case HNBU_LEDS:
					for (j = 1; j <= 4; j++) {
						if (cis[i + j] != 0xff) {
							varbuf_append(&b, vstr_ledbh, j-1,
							cis[i + j]);
						}
					}
					break;

				case HNBU_CCODE:
					ASSERT(sromrev > 1);
					if ((cis[i + 1] == 0) || (cis[i + 2] == 0))
						varbuf_append(&b, vstr_noccode);
					else
						varbuf_append(&b, vstr_ccode,
						              cis[i + 1], cis[i + 2]);
					varbuf_append(&b, vstr_cctl, cis[i + 3]);
					break;

				case HNBU_CCKPO:
					ASSERT(sromrev > 2);
					varbuf_append(&b, vstr_cckpo,
					              (cis[i + 2] << 8) | cis[i + 1]);
					break;

				case HNBU_OFDMPO:
					ASSERT(sromrev > 2);
					varbuf_append(&b, vstr_ofdmpo,
					              (cis[i + 4] << 24) |
					              (cis[i + 3] << 16) |
					              (cis[i + 2] << 8) |
					              cis[i + 1]);
					break;

				case HNBU_WPS:
					varbuf_append(&b, vstr_wpsgpio, cis[i + 1]);
					if (tlen >= 3)
						varbuf_append(&b, vstr_wpsled, cis[i + 2]);
					break;

				case HNBU_RSSISMBXA2G:
					ASSERT(sromrev == 3);
					varbuf_append(&b, vstr_rssismf2g, cis[i + 1] & 0xf);
					varbuf_append(&b, vstr_rssismc2g, (cis[i + 1] >> 4) & 0xf);
					varbuf_append(&b, vstr_rssisav2g, cis[i + 2] & 0x7);
					varbuf_append(&b, vstr_bxa2g, (cis[i + 2] >> 3) & 0x3);
					break;

				case HNBU_RSSISMBXA5G:
					ASSERT(sromrev == 3);
					varbuf_append(&b, vstr_rssismf5g, cis[i + 1] & 0xf);
					varbuf_append(&b, vstr_rssismc5g, (cis[i + 1] >> 4) & 0xf);
					varbuf_append(&b, vstr_rssisav5g, cis[i + 2] & 0x7);
					varbuf_append(&b, vstr_bxa5g, (cis[i + 2] >> 3) & 0x3);
					break;

				case HNBU_TRI2G:
					ASSERT(sromrev == 3);
					varbuf_append(&b, vstr_tri2g, cis[i + 1]);
					break;

				case HNBU_TRI5G:
					ASSERT(sromrev == 3);
					varbuf_append(&b, vstr_tri5gl, cis[i + 1]);
					varbuf_append(&b, vstr_tri5g, cis[i + 2]);
					varbuf_append(&b, vstr_tri5gh, cis[i + 3]);
					break;

				case HNBU_RXPO2G:
					ASSERT(sromrev == 3);
					varbuf_append(&b, vstr_rxpo2g, cis[i + 1]);
					break;

				case HNBU_RXPO5G:
					ASSERT(sromrev == 3);
					varbuf_append(&b, vstr_rxpo5g, cis[i + 1]);
					break;

				case HNBU_MACADDR:
					if (!(ETHER_ISNULLADDR(&cis[i+1])) &&
					    !(ETHER_ISMULTI(&cis[i+1]))) {
						bcm_ether_ntoa((struct ether_addr *)&cis[i + 1],
						               eabuf);

						/* set boardnum if HNBU_BOARDNUM not seen yet */
						if (boardnum == -1)
							boardnum = (cis[i + 5] << 8) + cis[i + 6];
					}
					break;

				case HNBU_LEDDC:
					/* CIS leddc only has 16bits, convert it to 32bits */
					w32 = ((cis[i + 2] << 24) | /* oncount */
					       (cis[i + 1] << 8)); /* offcount */
					varbuf_append(&b, vstr_leddc, w32);
					break;

				case HNBU_CHAINSWITCH:
					varbuf_append(&b, vstr_txchain, cis[i + 1]);
					varbuf_append(&b, vstr_rxchain, cis[i + 2]);
					varbuf_append(&b, vstr_antswitch,
					      (cis[i + 4] << 8) + cis[i + 3]);
					break;

				case HNBU_REGREV:
					varbuf_append(&b, vstr_regrev, cis[i + 1]);
					break;

				case HNBU_FEM: {
					uint16 fem = (cis[i + 2] << 8) + cis[i + 1];
					varbuf_append(&b, vstr_antswctl2g, (fem &
						SROM8_FEM_ANTSWLUT_MASK) >>
						SROM8_FEM_ANTSWLUT_SHIFT);
					varbuf_append(&b, vstr_triso2g, (fem &
						SROM8_FEM_TR_ISO_MASK) >>
						SROM8_FEM_TR_ISO_SHIFT);
					varbuf_append(&b, vstr_pdetrange2g, (fem &
						SROM8_FEM_PDET_RANGE_MASK) >>
						SROM8_FEM_PDET_RANGE_SHIFT);
					varbuf_append(&b, vstr_extpagain2g, (fem &
						SROM8_FEM_EXTPA_GAIN_MASK) >>
						SROM8_FEM_EXTPA_GAIN_SHIFT);
					varbuf_append(&b, vstr_tssipos2g, (fem &
						SROM8_FEM_TSSIPOS_MASK) >>
						SROM8_FEM_TSSIPOS_SHIFT);
					if (tlen < 5) break;

					fem = (cis[i + 4] << 8) + cis[i + 3];
					varbuf_append(&b, vstr_antswctl5g, (fem &
						SROM8_FEM_ANTSWLUT_MASK) >>
						SROM8_FEM_ANTSWLUT_SHIFT);
					varbuf_append(&b, vstr_triso5g, (fem &
						SROM8_FEM_TR_ISO_MASK) >>
						SROM8_FEM_TR_ISO_SHIFT);
					varbuf_append(&b, vstr_pdetrange5g, (fem &
						SROM8_FEM_PDET_RANGE_MASK) >>
						SROM8_FEM_PDET_RANGE_SHIFT);
					varbuf_append(&b, vstr_extpagain5g, (fem &
						SROM8_FEM_EXTPA_GAIN_MASK) >>
						SROM8_FEM_EXTPA_GAIN_SHIFT);
					varbuf_append(&b, vstr_tssipos5g, (fem &
						SROM8_FEM_TSSIPOS_MASK) >>
						SROM8_FEM_TSSIPOS_SHIFT);
					break;
					}

				case HNBU_PAPARMS_C0:
					varbuf_append(&b, vstr_maxp2ga0, cis[i + 1]);
					varbuf_append(&b, vstr_itt2ga0, cis[i + 2]);
					varbuf_append(&b, vstr_pa, 2, 0, 0,
						(cis[i + 4] << 8) + cis[i + 3]);
					varbuf_append(&b, vstr_pa, 2, 1, 0,
						(cis[i + 6] << 8) + cis[i + 5]);
					varbuf_append(&b, vstr_pa, 2, 2, 0,
						(cis[i + 8] << 8) + cis[i + 7]);
					if (tlen < 31) break;

					varbuf_append(&b, vstr_maxp5ga0, cis[i + 9]);
					varbuf_append(&b, vstr_itt5ga0, cis[i + 10]);
					varbuf_append(&b, vstr_maxp5gha0, cis[i + 11]);
					varbuf_append(&b, vstr_maxp5gla0, cis[i + 12]);
					varbuf_append(&b, vstr_pa, 5, 0, 0,
						(cis[i + 14] << 8) + cis[i + 13]);
					varbuf_append(&b, vstr_pa, 5, 1, 0,
						(cis[i + 16] << 8) + cis[i + 15]);
					varbuf_append(&b, vstr_pa, 5, 2, 0,
						(cis[i + 18] << 8) + cis[i + 17]);
					varbuf_append(&b, vstr_pahl, 5, 'l', 0, 0,
						(cis[i + 20] << 8) + cis[i + 19]);
					varbuf_append(&b, vstr_pahl, 5, 'l', 1, 0,
						(cis[i + 22] << 8) + cis[i + 21]);
					varbuf_append(&b, vstr_pahl, 5, 'l', 2, 0,
						(cis[i + 24] << 8) + cis[i + 23]);
					varbuf_append(&b, vstr_pahl, 5, 'h', 0, 0,
						(cis[i + 26] << 8) + cis[i + 25]);
					varbuf_append(&b, vstr_pahl, 5, 'h', 1, 0,
						(cis[i + 28] << 8) + cis[i + 27]);
					varbuf_append(&b, vstr_pahl, 5, 'h', 2, 0,
						(cis[i + 30] << 8) + cis[i + 29]);
					break;

				case HNBU_PAPARMS_C1:
					varbuf_append(&b, vstr_maxp2ga1, cis[i + 1]);
					varbuf_append(&b, vstr_itt2ga1, cis[i + 2]);
					varbuf_append(&b, vstr_pa, 2, 0, 1,
						(cis[i + 4] << 8) + cis[i + 3]);
					varbuf_append(&b, vstr_pa, 2, 1, 1,
						(cis[i + 6] << 8) + cis[i + 5]);
					varbuf_append(&b, vstr_pa, 2, 2, 1,
						(cis[i + 8] << 8) + cis[i + 7]);
					if (tlen < 31) break;

					varbuf_append(&b, vstr_maxp5ga1, cis[i + 9]);
					varbuf_append(&b, vstr_itt5ga1, cis[i + 10]);
					varbuf_append(&b, vstr_maxp5gha1, cis[i + 11]);
					varbuf_append(&b, vstr_maxp5gla1, cis[i + 12]);
					varbuf_append(&b, vstr_pa, 5, 0, 1,
						(cis[i + 14] << 8) + cis[i + 13]);
					varbuf_append(&b, vstr_pa, 5, 1, 1,
						(cis[i + 16] << 8) + cis[i + 15]);
					varbuf_append(&b, vstr_pa, 5, 2, 1,
						(cis[i + 18] << 8) + cis[i + 17]);
					varbuf_append(&b, vstr_pahl, 5, 'l', 0, 1,
						(cis[i + 20] << 8) + cis[i + 19]);
					varbuf_append(&b, vstr_pahl, 5, 'l', 1, 1,
						(cis[i + 22] << 8) + cis[i + 21]);
					varbuf_append(&b, vstr_pahl, 5, 'l', 2, 1,
						(cis[i + 24] << 8) + cis[i + 23]);
					varbuf_append(&b, vstr_pahl, 5, 'h', 0, 1,
						(cis[i + 26] << 8) + cis[i + 25]);
					varbuf_append(&b, vstr_pahl, 5, 'h', 1, 1,
						(cis[i + 28] << 8) + cis[i + 27]);
					varbuf_append(&b, vstr_pahl, 5, 'h', 2, 1,
						(cis[i + 30] << 8) + cis[i + 29]);
					break;

				case HNBU_PO_CCKOFDM:
					varbuf_append(&b, vstr_cck2gpo,
						(cis[i + 2] << 8) + cis[i + 1]);
					varbuf_append(&b, vstr_ofdm2gpo,
						(cis[i + 6] << 24) + (cis[i + 5] << 16) +
						(cis[i + 4] << 8) + cis[i + 3]);
					if (tlen < 19) break;

					varbuf_append(&b, vstr_ofdm5gpo,
						(cis[i + 10] << 24) + (cis[i + 9] << 16) +
						(cis[i + 8] << 8) + cis[i + 7]);
					varbuf_append(&b, vstr_ofdm5glpo,
						(cis[i + 14] << 24) + (cis[i + 13] << 16) +
						(cis[i + 12] << 8) + cis[i + 11]);
					varbuf_append(&b, vstr_ofdm5ghpo,
						(cis[i + 18] << 24) + (cis[i + 17] << 16) +
						(cis[i + 16] << 8) + cis[i + 15]);
					break;

				case HNBU_PO_MCS2G:
					for (j = 0; j <= (tlen/2); j++) {
						varbuf_append(&b, vstr_mcspo, 2, j,
							(cis[i + 2 + 2*j] << 8) + cis[i + 1 + 2*j]);
					}
					break;

				case HNBU_PO_MCS5GM:
					for (j = 0; j <= (tlen/2); j++) {
						varbuf_append(&b, vstr_mcspo, 5, j,
							(cis[i + 2 + 2*j] << 8) + cis[i + 1 + 2*j]);
					}
					break;

				case HNBU_PO_MCS5GLH:
					for (j = 0; j <= (tlen/4); j++) {
						varbuf_append(&b, vstr_mcspohl, 5, 'l', j,
							(cis[i + 2 + 2*j] << 8) + cis[i + 1 + 2*j]);
					}

					for (j = 0; j <= (tlen/4); j++) {
						varbuf_append(&b, vstr_mcspohl, 5, 'h', j,
							(cis[i + ((tlen/2)+2) + 2*j] << 8) +
							cis[i + ((tlen/2)+1) + 2*j]);
					}

					break;

				case HNBU_PO_CDD:
					varbuf_append(&b, vstr_cddpo,
					              (cis[i + 2] << 8) + cis[i + 1]);
					break;

				case HNBU_PO_STBC:
					varbuf_append(&b, vstr_stbcpo,
					              (cis[i + 2] << 8) + cis[i + 1]);
					break;

				case HNBU_PO_40M:
					varbuf_append(&b, vstr_bw40po,
					              (cis[i + 2] << 8) + cis[i + 1]);
					break;

				case HNBU_PO_40MDUP:
					varbuf_append(&b, vstr_bwduppo,
					              (cis[i + 2] << 8) + cis[i + 1]);
					break;

				case HNBU_OFDMPO5G:
					varbuf_append(&b, vstr_ofdm5gpo,
						(cis[i + 4] << 24) + (cis[i + 3] << 16) +
						(cis[i + 2] << 8) + cis[i + 1]);
					varbuf_append(&b, vstr_ofdm5glpo,
						(cis[i + 8] << 24) + (cis[i + 7] << 16) +
						(cis[i + 6] << 8) + cis[i + 5]);
					varbuf_append(&b, vstr_ofdm5ghpo,
						(cis[i + 12] << 24) + (cis[i + 11] << 16) +
						(cis[i + 10] << 8) + cis[i + 9]);
					break;
				/* Power per rate for SROM V9 */
				case HNBU_CCKBW202GPO:
					varbuf_append(&b, vstr_cckbw202gpo[0],
						((cis[i + 2] << 8) + cis[i + 1]));
					if (tlen > 4)
						varbuf_append(&b, vstr_cckbw202gpo[1],
							((cis[i + 4] << 8) + cis[i + 3]));
					break;

				case HNBU_LEGOFDMBW202GPO:
					varbuf_append(&b, vstr_legofdmbw202gpo[0],
						((cis[i + 4] << 24) + (cis[i + 3] << 16) +
						(cis[i + 2] << 8) + cis[i + 1]));
					if (tlen > 6)  {
						varbuf_append(&b, vstr_legofdmbw202gpo[1],
							((cis[i + 8] << 24) + (cis[i + 7] << 16) +
							(cis[i + 6] << 8) + cis[i + 5]));
					}
					break;

				case HNBU_LEGOFDMBW205GPO:
					for (j = 0; j < 6; j++) {
						if (tlen < (2 + 4 * j))
							break;
						varbuf_append(&b, vstr_legofdmbw205gpo[j],
							((cis[4 * j + i + 4] << 24)
							+ (cis[4 * j + i + 3] << 16)
							+ (cis[4 * j + i + 2] << 8)
							+ cis[4 * j + i + 1]));
					}
					break;

				case HNBU_MCS2GPO:
					for (j = 0; j < 3; j++) {
						if (tlen < (2 + 4 * j))
							break;
						varbuf_append(&b, vstr_mcs2gpo[j],
							((cis[4 * j + i + 4] << 24)
							+ (cis[4 * j + i + 3] << 16)
							+ (cis[4 * j + i + 2] << 8)
							+ cis[4 * j + i + 1]));
					}
					break;

				case HNBU_MCS5GLPO:
					for (j = 0; j < 3; j++) {
						if (tlen < (2 + 4 * j))
							break;
						varbuf_append(&b, vstr_mcs5glpo[j],
							((cis[4 * j + i + 4] << 24)
							+ (cis[4 * j + i + 3] << 16)
							+ (cis[4 * j + i + 2] << 8)
							+ cis[4 * j + i + 1]));
					}
					break;

				case HNBU_MCS5GMPO:
					for (j = 0; j < 3; j++) {
						if (tlen < (2 + 4 * j))
							break;
						varbuf_append(&b, vstr_mcs5gmpo[j],
							((cis[4 * j + i + 4] << 24)
							+ (cis[4 * j + i + 3] << 16)
							+ (cis[4 * j + i + 2] << 8)
							+ cis[4 * j + i + 1]));
					}
					break;

				case HNBU_MCS5GHPO:
					for (j = 0; j < 3; j++) {
						if (tlen < (2 + 4 * j))
							break;
						varbuf_append(&b, vstr_mcs5ghpo[j],
							((cis[4 * j + i + 4] << 24)
							+ (cis[4 * j + i + 3] << 16)
							+ (cis[4 * j + i + 2] << 8)
							+ cis[4 * j + i + 1]));
					}
					break;

				case HNBU_MCS32PO:
					varbuf_append(&b, vstr_mcs32po,
						(cis[i + 2] << 8) + cis[i + 1]);
					break;

				case HNBU_LEG40DUPPO:
					varbuf_append(&b, vstr_legofdm40duppo,
						(cis[i + 2] << 8) + cis[i + 1]);
					break;

				case HNBU_CUSTOM1:
					varbuf_append(&b, vstr_custom, 1, ((cis[i + 4] << 24) +
						(cis[i + 3] << 16) + (cis[i + 2] << 8) +
						cis[i + 1]));
					break;

#if defined(BCMCCISSR3)
				case HNBU_SROM3SWRGN:
					if (tlen >= 73) {
						uint16 srom[35];
						uint8 srev = cis[i + 1 + 70];
						ASSERT(srev == 3);
						/* make tuple value 16-bit aligned and parse it */
						bcopy(&cis[i + 1], srom, sizeof(srom));
						_initvars_srom_pci(srev, srom, SROM3_SWRGN_OFF, &b);
						/* 2.4G antenna gain is included in SROM */
						ag_init = TRUE;
						/* Ethernet MAC address is included in SROM */
						eabuf[0] = 0;
						boardnum = -1;
					}
					/* create extra variables */
					if (tlen >= 75)
						varbuf_append(&b, vstr_vendid,
						              (cis[i + 1 + 73] << 8) +
						              cis[i + 1 + 72]);
					if (tlen >= 77)
						varbuf_append(&b, vstr_devid,
						              (cis[i + 1 + 75] << 8) +
						              cis[i + 1 + 74]);
					if (tlen >= 79)
						varbuf_append(&b, vstr_xtalfreq,
						              (cis[i + 1 + 77] << 8) +
						              cis[i + 1 + 76]);
					break;
#endif	

				case HNBU_CCKFILTTYPE:
					varbuf_append(&b, vstr_cckdigfilttype,
						(cis[i + 1]));
					break;

				case HNBU_TEMPTHRESH:
					varbuf_append(&b, vstr_tempthresh,
						(cis[i + 1]));
					/* period in msb nibble */
					varbuf_append(&b, vstr_temps_period,
						(cis[i + 2]  >> 4));
					/* hysterisis in lsb nibble */
					varbuf_append(&b, vstr_temp_hysteresis,
						(cis[i + 2] & 0xF));
					break;
				case HNBU_UUID:
					{
					/* uuid format 12345678-1234-5678-1234-567812345678 */

					char uuidstr[37]; /* 32 ids, 4 '-', 1 Null */  

					snprintf(uuidstr, sizeof(uuidstr),
						"%02X%02X%02X%02X-%02X%02X-%02X%02X-"
						"%02X%02X-%02X%02X%02X%02X%02X%02X",
						cis[i + 1], cis[i + 2], cis[i + 3], cis[i + 4],
						cis[i + 5], cis[i + 6], cis[i + 7], cis[i + 8],
						cis[i + 9], cis[i + 10], cis[i + 11], cis[i + 12],
						cis[i + 13], cis[i + 14], cis[i + 15], cis[i + 16]);

					varbuf_append(&b, vstr_uuid, uuidstr);
					break;

					}
#endif /* !BCM_BOOTLOADER */
				}

				break;
			}
			i += tlen;
		} while (tup != CISTPL_END);
	}

	if (boardnum != -1) {
		varbuf_append(&b, vstr_boardnum, boardnum);
	}

	if (eabuf[0]) {
		varbuf_append(&b, vstr_macaddr, eabuf);
	}

#ifndef BCM_BOOTLOADER
	/* if there is no antenna gain field, set default */
	if (getvar(NULL, "ag0") == NULL && ag_init == FALSE) {
		varbuf_append(&b, vstr_ag, 0, 0xff);
	}
#endif

#if defined(BCMUSBDEV_BMAC) || defined(BCM_BMAC_VARS_APPEND)
	varbuf_append(&b, vstr_end, NULL);
#endif /*  BCMUSBDEV_BMAC */

	/* final nullbyte terminator */
	ASSERT(b.size >= 1);
	*b.buf++ = '\0';

	ASSERT(b.buf - base <= MAXSZ_NVRAM_VARS);
	err = initvars_table(osh, base, b.buf, vars, count);

	MFREE(osh, base, MAXSZ_NVRAM_VARS);
	return err;
}

/* set PCMCIA sprom command register */
static int
sprom_cmd_pcmcia(osl_t *osh, uint8 cmd)
{
	uint8 status = 0;
	uint wait_cnt = 1000;

	/* write sprom command register */
	OSL_PCMCIA_WRITE_ATTR(osh, SROM_CS, &cmd, 1);

	/* wait status */
	while (wait_cnt--) {
		OSL_PCMCIA_READ_ATTR(osh, SROM_CS, &status, 1);
		if (status & SROM_DONE)
			return 0;
	}

	return 1;
}

/* read a word from the PCMCIA srom */
static int
sprom_read_pcmcia(osl_t *osh, uint16 addr, uint16 *data)
{
	uint8 addr_l, addr_h, data_l, data_h;

	addr_l = (uint8)((addr * 2) & 0xff);
	addr_h = (uint8)(((addr * 2) >> 8) & 0xff);

	/* set address */
	OSL_PCMCIA_WRITE_ATTR(osh, SROM_ADDRH, &addr_h, 1);
	OSL_PCMCIA_WRITE_ATTR(osh, SROM_ADDRL, &addr_l, 1);

	/* do read */
	if (sprom_cmd_pcmcia(osh, SROM_READ))
		return 1;

	/* read data */
	data_h = data_l = 0;
	OSL_PCMCIA_READ_ATTR(osh, SROM_DATAH, &data_h, 1);
	OSL_PCMCIA_READ_ATTR(osh, SROM_DATAL, &data_l, 1);

	*data = (data_h << 8) | data_l;
	return 0;
}

#if defined(WLTEST) || defined(DHD_SPROM) || defined(BCMDBG)
/* write a word to the PCMCIA srom */
static int
sprom_write_pcmcia(osl_t *osh, uint16 addr, uint16 data)
{
	uint8 addr_l, addr_h, data_l, data_h;

	addr_l = (uint8)((addr * 2) & 0xff);
	addr_h = (uint8)(((addr * 2) >> 8) & 0xff);
	data_l = (uint8)(data & 0xff);
	data_h = (uint8)((data >> 8) & 0xff);

	/* set address */
	OSL_PCMCIA_WRITE_ATTR(osh, SROM_ADDRH, &addr_h, 1);
	OSL_PCMCIA_WRITE_ATTR(osh, SROM_ADDRL, &addr_l, 1);

	/* write data */
	OSL_PCMCIA_WRITE_ATTR(osh, SROM_DATAH, &data_h, 1);
	OSL_PCMCIA_WRITE_ATTR(osh, SROM_DATAL, &data_l, 1);

	/* do write */
	return sprom_cmd_pcmcia(osh, SROM_WRITE);
}
#endif 

/* In chips with chipcommon rev 32 and later, the srom is in chipcommon,
 * not in the bus cores.
 */
static uint16
srom_cc_cmd(si_t *sih, osl_t *osh, void *ccregs, uint32 cmd, uint wordoff, uint16 data)
{
	chipcregs_t *cc = (chipcregs_t *)ccregs;
	uint wait_cnt = 1000;

	if ((cmd == SRC_OP_READ) || (cmd == SRC_OP_WRITE)) {
		W_REG(osh, &cc->sromaddress, wordoff * 2);
		if (cmd == SRC_OP_WRITE)
			W_REG(osh, &cc->sromdata, data);
	}

	W_REG(osh, &cc->sromcontrol, SRC_START | cmd);

	while (wait_cnt--) {
		if ((R_REG(osh, &cc->sromcontrol) & SRC_BUSY) == 0)
			break;
	}

	if (!wait_cnt) {
		BS_ERROR(("%s: Command 0x%x timed out\n", __FUNCTION__, cmd));
		return 0xffff;
	}
	if (cmd == SRC_OP_READ)
		return (uint16)R_REG(osh, &cc->sromdata);
	else
		return 0xffff;
}

/*
 * Read in and validate sprom.
 * Return 0 on success, nonzero on error.
 */
static int
sprom_read_pci(osl_t *osh, si_t *sih, uint16 *sprom, uint wordoff, uint16 *buf, uint nwords,
	bool check_crc)
{
	int err = 0;
	uint i;
	void *ccregs = NULL;
	uint32 ccval = 0;

	if ((CHIPID(sih->chip) == BCM4331_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43431_CHIP_ID)) {
		/* save current control setting */
		ccval = si_chipcontrl_epa4331_read(sih);
		/* Disable Ext PA lines to allow reading from SROM */
		si_chipcontrl_epa4331(sih, FALSE);
	}

	/* read the sprom */
	for (i = 0; i < nwords; i++) {

		if (sih->ccrev > 31 && ISSIM_ENAB(sih)) {
			/* use indirect since direct is too slow on QT */
			if ((sih->cccaps & CC_CAP_SROM) == 0) {
				err = 1;
				goto error;
			}

			ccregs = (void *)((uint8 *)sprom - CC_SROM_OTP);
			buf[i] = srom_cc_cmd(sih, osh, ccregs, SRC_OP_READ, wordoff + i, 0);

		} else {
			if (ISSIM_ENAB(sih))
				buf[i] = R_REG(osh, &sprom[wordoff + i]);

			buf[i] = R_REG(osh, &sprom[wordoff + i]);
		}

	}

	/* bypass crc checking for simulation to allow srom hack */
	if (ISSIM_ENAB(sih))
		goto error;

	if (check_crc) {

		if (buf[0] == 0xffff) {
			/* The hardware thinks that an srom that starts with 0xffff
			 * is blank, regardless of the rest of the content, so declare
			 * it bad.
			 */
			BS_ERROR(("%s: buf[0] = 0x%x, returning bad-crc\n", __FUNCTION__, buf[0]));
			err = 1;
			goto error;
		}

		/* fixup the endianness so crc8 will pass */
		htol16_buf(buf, nwords * 2);
		if (hndcrc8((uint8 *)buf, nwords * 2, CRC8_INIT_VALUE) != CRC8_GOOD_VALUE) {
			/* DBG only pci always read srom4 first, then srom8/9 */
			/* BS_ERROR(("%s: bad crc\n", __FUNCTION__)); */
			err = 1;
		}
		/* now correct the endianness of the byte array */
		ltoh16_buf(buf, nwords * 2);
	}

error:
	if ((CHIPID(sih->chip) == BCM4331_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43431_CHIP_ID)) {
		/* Restore config after reading SROM */
		si_chipcontrl_epa4331_restore(sih, ccval);
	}

	return err;
}

#if defined(BCMNVRAMW) || defined(BCMNVRAMR)
static int
otp_read_pci(osl_t *osh, si_t *sih, uint16 *buf, uint bufsz)
{
	uint8 *otp;
	uint sz = OTP_SZ_MAX/2; /* size in words */
	int err = 0;

	ASSERT(bufsz <= OTP_SZ_MAX);

	if ((otp = MALLOC(osh, OTP_SZ_MAX)) == NULL) {
		return BCME_ERROR;
	}

	bzero(otp, OTP_SZ_MAX);

	err = otp_read_region(sih, OTP_HW_RGN, (uint16 *)otp, &sz);

	bcopy(otp, buf, bufsz);

	if (otp)
		MFREE(osh, otp, OTP_SZ_MAX);

	/* Check CRC */
	if (buf[0] == 0xffff) {
		/* The hardware thinks that an srom that starts with 0xffff
		 * is blank, regardless of the rest of the content, so declare
		 * it bad.
		 */
		BS_ERROR(("%s: buf[0] = 0x%x, returning bad-crc\n", __FUNCTION__, buf[0]));
		return 1;
	}

	/* fixup the endianness so crc8 will pass */
	htol16_buf(buf, bufsz);
	if (hndcrc8((uint8 *)buf, SROM4_WORDS * 2, CRC8_INIT_VALUE) != CRC8_GOOD_VALUE) {
		BS_ERROR(("%s: bad crc\n", __FUNCTION__));
		err = 1;
	}
	/* now correct the endianness of the byte array */
	ltoh16_buf(buf, bufsz);

	return err;
}
#endif /* defined(BCMNVRAMW) || defined(BCMNVRAMR) */

#if defined(WLTEST) || defined(BCMDBG)
int
srom_otp_write_region_crc(si_t *sih, uint nbytes, uint16* buf16, bool write)
{
	int err = 0, crc = 0;
	uint8 *buf8;

	/* Check nbytes is not odd or too big */
	if ((nbytes & 1) || (nbytes > SROM_MAX))
		return 1;

	/* block invalid buffer size */
	if (nbytes < SROM4_WORDS * 2)
		return BCME_BUFTOOSHORT;
	else if (nbytes > SROM4_WORDS * 2)
		return BCME_BUFTOOLONG;

	/* Verify signatures */
	if (!((buf16[SROM4_SIGN] == SROM4_SIGNATURE) ||
		(buf16[SROM8_SIGN] == SROM4_SIGNATURE))) {
		BS_ERROR(("%s: wrong signature SROM4_SIGN %x SROM8_SIGN %x\n",
			__FUNCTION__, buf16[SROM4_SIGN], buf16[SROM8_SIGN]));
		return BCME_ERROR;
	}

	/* Check CRC */
	if (buf16[0] == 0xffff) {
		/* The hardware thinks that an srom that starts with 0xffff
		 * is blank, regardless of the rest of the content, so declare
		 * it bad.
		 */
		BS_ERROR(("%s: invalid buf16[0] = 0x%x\n", __FUNCTION__, buf16[0]));
		goto out;
	}

	buf8 = (uint8*)buf16;
	/* fixup the endianness and then calculate crc */
	htol16_buf(buf8, nbytes);
	crc = ~hndcrc8(buf8, nbytes - 1, CRC8_INIT_VALUE);
	/* now correct the endianness of the byte array */
	ltoh16_buf(buf8, nbytes);
	buf16[SROM4_CRCREV] = (crc << 8) | (buf16[SROM4_CRCREV] & 0xff);

#ifdef BCMNVRAMW
	/* Write the CRC back */
	if (write)
		err = otp_cis_append_region(sih, OTP_HW_RGN, (char*)buf8, (int)nbytes);
#endif /* BCMNVRAMW */

out:
	return write ? err : crc;
}
#endif 

/*
* Create variable table from memory.
* Return 0 on success, nonzero on error.
*/
static int
BCMATTACHFN(initvars_table)(osl_t *osh, char *start, char *end, char **vars, uint *count)
{
	int c = (int)(end - start);

	/* do it only when there is more than just the null string */
	if (c > 1) {
		char *vp = MALLOC(osh, c);
		ASSERT(vp != NULL);
		if (!vp)
			return BCME_NOMEM;
		bcopy(start, vp, c);
		*vars = vp;
		*count = c;
	}
	else {
		*vars = NULL;
		*count = 0;
	}

	return 0;
}

/*
 * Find variables with <devpath> from flash. 'base' points to the beginning
 * of the table upon enter and to the end of the table upon exit when success.
 * Return 0 on success, nonzero on error.
 */
static int
BCMATTACHFN(initvars_flash)(si_t *sih, osl_t *osh, char **base, uint len)
{
	char *vp = *base;
	char *flash;
	int err;
	char *s;
	uint l, dl, copy_len;
	char devpath[SI_DEVPATH_BUFSZ];
	char coded_name[SI_DEVPATH_BUFSZ] = {0};
	int path_len, coded_len, devid_len;

	/* allocate memory and read in flash */
	if (!(flash = MALLOC(osh, NVRAM_SPACE)))
		return BCME_NOMEM;
	if ((err = nvram_getall(flash, NVRAM_SPACE)))
		goto exit;

	/* create legacy devpath prefix */
	si_devpath(sih, devpath, sizeof(devpath));
	path_len = strlen(devpath);

	/* create coded devpath prefix */
	si_coded_devpathvar(sih, coded_name, sizeof(coded_name), "devid");

	/* coded_name now is 'xx:devid, eat ending 'devid' */
	/* to be 'xx:' */
	devid_len = strlen("devid");
	coded_len = strlen(coded_name);
	if (coded_len > devid_len) {
		coded_name[coded_len - devid_len] = '\0';
		coded_len -= devid_len;
	}
	else
		coded_len = 0;

	/* grab vars with the <devpath> prefix or <coded_name> previx in name */
	for (s = flash; s && *s; s += l + 1) {
		l = strlen(s);

		/* skip non-matching variable */
		if (strncmp(s, devpath, path_len) == 0)
			dl = path_len;
		else if (coded_len && strncmp(s, coded_name, coded_len) == 0)
			dl = coded_len;
		else
			continue;

		/* is there enough room to copy? */
		copy_len = l - dl + 1;
		if (len < copy_len) {
			err = BCME_BUFTOOSHORT;
			goto exit;
		}

		/* no prefix, just the name=value */
		strncpy(vp, &s[dl], copy_len);
		vp += copy_len;
		len -= copy_len;
	}

	/* add null string as terminator */
	if (len < 1) {
		err = BCME_BUFTOOSHORT;
		goto exit;
	}
	*vp++ = '\0';

	*base = vp;

exit:	MFREE(osh, flash, NVRAM_SPACE);
	return err;
}

#if !defined(BCMUSBDEV)
/*
 * Initialize nonvolatile variable table from flash.
 * Return 0 on success, nonzero on error.
 */
static int
BCMATTACHFN(initvars_flash_si)(si_t *sih, char **vars, uint *count)
{
	osl_t *osh = si_osh(sih);
	char *vp, *base;
	int err;

	ASSERT(vars != NULL);
	ASSERT(count != NULL);

	base = vp = MALLOC(osh, MAXSZ_NVRAM_VARS);
	ASSERT(vp != NULL);
	if (!vp)
		return BCME_NOMEM;

	if ((err = initvars_flash(sih, osh, &vp, MAXSZ_NVRAM_VARS)) == 0)
		err = initvars_table(osh, base, vp, vars, count);

	MFREE(osh, base, MAXSZ_NVRAM_VARS);

	return err;
}
#endif	

/* Parse SROM and create name=value pairs. 'srom' points to
 * the SROM word array. 'off' specifies the offset of the
 * first word 'srom' points to, which should be either 0 or
 * SROM3_SWRG_OFF (full SROM or software region).
 */

static uint
mask_shift(uint16 mask)
{
	uint i;
	for (i = 0; i < (sizeof(mask) << 3); i ++) {
		if (mask & (1 << i))
			return i;
	}
	ASSERT(mask);
	return 0;
}

static uint
mask_width(uint16 mask)
{
	int i;
	for (i = (sizeof(mask) << 3) - 1; i >= 0; i --) {
		if (mask & (1 << i))
			return (uint)(i - mask_shift(mask) + 1);
	}
	ASSERT(mask);
	return 0;
}

#ifdef BCMASSERT_SUPPORT
static bool
mask_valid(uint16 mask)
{
	uint shift = mask_shift(mask);
	uint width = mask_width(mask);
	return mask == ((~0 << shift) & ~(~0 << (shift + width)));
}
#endif

static void
BCMATTACHFN(_initvars_srom_pci)(uint8 sromrev, uint16 *srom, uint off, varbuf_t *b)
{
	uint16 w;
	uint32 val;
	const sromvar_t *srv;
	uint width;
	uint flags;
	uint32 sr = (1 << sromrev);

	varbuf_append(b, "sromrev=%d", sromrev);

	for (srv = pci_sromvars; srv->name != NULL; srv ++) {
		const char *name;

		if ((srv->revmask & sr) == 0)
			continue;

		if (srv->off < off)
			continue;

		flags = srv->flags;
		name = srv->name;

		/* This entry is for mfgc only. Don't generate param for it, */
		if (flags & SRFL_NOVAR)
			continue;

		if (flags & SRFL_ETHADDR) {
			char eabuf[ETHER_ADDR_STR_LEN];
			struct ether_addr ea;

			ea.octet[0] = (srom[srv->off - off] >> 8) & 0xff;
			ea.octet[1] = srom[srv->off - off] & 0xff;
			ea.octet[2] = (srom[srv->off + 1 - off] >> 8) & 0xff;
			ea.octet[3] = srom[srv->off + 1 - off] & 0xff;
			ea.octet[4] = (srom[srv->off + 2 - off] >> 8) & 0xff;
			ea.octet[5] = srom[srv->off + 2 - off] & 0xff;
			bcm_ether_ntoa(&ea, eabuf);

			varbuf_append(b, "%s=%s", name, eabuf);
		}
		else {
			ASSERT(mask_valid(srv->mask));
			ASSERT(mask_width(srv->mask));

			w = srom[srv->off - off];
			val = (w & srv->mask) >> mask_shift(srv->mask);
			width = mask_width(srv->mask);

			while (srv->flags & SRFL_MORE) {
				srv ++;
				ASSERT(srv->name != NULL);

				if (srv->off == 0 || srv->off < off)
					continue;

				ASSERT(mask_valid(srv->mask));
				ASSERT(mask_width(srv->mask));

				w = srom[srv->off - off];
				val += ((w & srv->mask) >> mask_shift(srv->mask)) << width;
				width += mask_width(srv->mask);
			}

			if ((flags & SRFL_NOFFS) && ((int)val == (1 << width) - 1))
				continue;

			if (flags & SRFL_CCODE) {
				if (val == 0)
					varbuf_append(b, "ccode=");
				else
					varbuf_append(b, "ccode=%c%c", (val >> 8), (val & 0xff));
			}
			/* LED Powersave duty cycle has to be scaled:
			 *(oncount >> 24) (offcount >> 8)
			 */
			else if (flags & SRFL_LEDDC) {
				uint32 w32 = (((val >> 8) & 0xff) << 24) | /* oncount */
					     (((val & 0xff)) << 8); /* offcount */
				varbuf_append(b, "leddc=%d", w32);
			}
			else if (flags & SRFL_PRHEX)
				varbuf_append(b, "%s=0x%x", name, val);
			else if ((flags & SRFL_PRSIGN) && (val & (1 << (width - 1))))
				varbuf_append(b, "%s=%d", name, (int)(val | (~0 << width)));
			else
				varbuf_append(b, "%s=%u", name, val);
		}
	}

	if (sromrev >= 4) {
		/* Do per-path variables */
		uint p, pb, psz;

		if (sromrev >= 8) {
			pb = SROM8_PATH0;
			psz = SROM8_PATH1 - SROM8_PATH0;
		} else {
			pb = SROM4_PATH0;
			psz = SROM4_PATH1 - SROM4_PATH0;
		}

		for (p = 0; p < MAX_PATH_SROM; p++) {
			for (srv = perpath_pci_sromvars; srv->name != NULL; srv ++) {
				if ((srv->revmask & sr) == 0)
					continue;

				if (pb + srv->off < off)
					continue;

				/* This entry is for mfgc only. Don't generate param for it, */
				if (srv->flags & SRFL_NOVAR)
					continue;

				w = srom[pb + srv->off - off];

				ASSERT(mask_valid(srv->mask));
				val = (w & srv->mask) >> mask_shift(srv->mask);
				width = mask_width(srv->mask);

				/* Cheating: no per-path var is more than 1 word */

				if ((srv->flags & SRFL_NOFFS) && ((int)val == (1 << width) - 1))
					continue;

				if (srv->flags & SRFL_PRHEX)
					varbuf_append(b, "%s%d=0x%x", srv->name, p, val);
				else
					varbuf_append(b, "%s%d=%d", srv->name, p, val);
			}
			pb += psz;
		}
	}
}

/*
 * Initialize nonvolatile variable table from sprom.
 * Return 0 on success, nonzero on error.
 */
static int
BCMATTACHFN(initvars_srom_pci)(si_t *sih, void *curmap, char **vars, uint *count)
{
	uint16 *srom, *sromwindow;
	uint8 sromrev = 0;
	uint32 sr;
	varbuf_t b;
	char *vp, *base = NULL;
	osl_t *osh = si_osh(sih);
	bool flash = FALSE;
	int err = 0;

	/*
	 * Apply CRC over SROM content regardless SROM is present or not,
	 * and use variable <devpath>sromrev's existance in flash to decide
	 * if we should return an error when CRC fails or read SROM variables
	 * from flash.
	 */
	srom = MALLOC(osh, SROM_MAX);
	ASSERT(srom != NULL);
	if (!srom)
		return -2;

	sromwindow = (uint16 *)SROM_OFFSET(sih);
	if (si_is_sprom_available(sih)) {
		err = sprom_read_pci(osh, sih, sromwindow, 0, srom, SROM_WORDS, TRUE);

		if ((srom[SROM4_SIGN] == SROM4_SIGNATURE) ||
		    (((sih->buscoretype == PCIE_CORE_ID) && (sih->buscorerev >= 6)) ||
		     ((sih->buscoretype == PCI_CORE_ID) && (sih->buscorerev >= 0xe)))) {
			/* sromrev >= 4, read more */
			err = sprom_read_pci(osh, sih, sromwindow, 0, srom, SROM4_WORDS, TRUE);
			sromrev = srom[SROM4_CRCREV] & 0xff;
			if (err)
				BS_ERROR(("%s: srom %d, bad crc\n", __FUNCTION__, sromrev));

		} else if (err == 0) {
			/* srom is good and is rev < 4 */
			/* top word of sprom contains version and crc8 */
			sromrev = srom[SROM_CRCREV] & 0xff;
			/* bcm4401 sroms misprogrammed */
			if (sromrev == 0x10)
				sromrev = 1;
		}
	}

#if defined(BCMNVRAMW) || defined(BCMNVRAMR)
	/* Use OTP if SPROM not available */
	else if ((err = otp_read_pci(osh, sih, srom, SROM_MAX)) == 0) {
		/* OTP only contain SROM rev8/rev9 for now */
		sromrev = srom[SROM4_CRCREV] & 0xff;
	}
#endif /* defined(BCMNVRAMW) || defined(BCMNVRAMR) */
	else {
		err = 1;
		BS_ERROR(("Neither SPROM nor OTP has valid image\n"));
	}


	/* We want internal/wltest driver to come up with default sromvars so we can
	 * program a blank SPROM/OTP.
	 */
	if (err) {
		char *value;
		uint32 val;
		val = 0;

		if ((value = si_getdevpathvar(sih, "sromrev"))) {
			sromrev = (uint8)bcm_strtoul(value, NULL, 0);
			flash = TRUE;
			goto varscont;
		}

		BS_ERROR(("%s, SROM CRC Error\n", __FUNCTION__));

#ifndef DONGLEBUILD
		if ((value = si_getnvramflvar(sih, "sromrev"))) {
			err = 0;
			goto errout;
		}
#endif
/* BCMHOSTVARS is enabled only if WLTEST is enabled or BCMEXTNVM is enabled */
#if defined(BCMHOSTVARS)
		val = OSL_PCI_READ_CONFIG(osh, PCI_SPROM_CONTROL, sizeof(uint32));
		if ((si_is_sprom_available(sih) && srom[0] == 0xffff) ||
			(val & SPROM_OTPIN_USE)) {
			vp = base = mfgsromvars;

			if (defvarslen == 0) {
				BS_ERROR(("No nvm file, use generic default (for programming"
					" SPROM/OTP only)\n"));

				if (((sih->chip == BCM4331_CHIP_ID) ||
					(sih->chip == BCM43431_CHIP_ID)) &&
					(sih->chiprev < 3)) {

					defvarslen = srom_vars_len(defaultsromvars_4331);
					bcopy(defaultsromvars_4331, vp, defvarslen);

				} else {
					/* For 4311 A1 there is no signature to indicate that OTP is
					 * programmed, so can't really verify the OTP is
					 * unprogrammed or a bad OTP.
					 */
					if (sih->chip == BCM4311_CHIP_ID) {
						const char *devid = "devid=0x4311";
						const size_t devid_strlen = strlen(devid);
						BS_ERROR(("setting the devid to be 4311\n"));
						bcopy(devid, vp, devid_strlen + 1);
						vp += devid_strlen + 1;
					}
					defvarslen = srom_vars_len(defaultsromvars_wltest);
					bcopy(defaultsromvars_wltest, vp, defvarslen);
				}
			} else {
				BS_ERROR(("Use nvm file as default\n"));
			}

			vp += defvarslen;
			/* add final null terminator */
			*vp++ = '\0';

			BS_ERROR(("Used %d bytes of defaultsromvars\n", defvarslen));
			goto varsdone;

		} else if (((sih->chip == BCM4331_CHIP_ID) ||
			(sih->chip == BCM43431_CHIP_ID)) &&
			(sih->chiprev < 3)) {
			base = vp = mfgsromvars;

			BS_ERROR(("4331 BOOT w/o SPROM or OTP\n"));

			defvarslen = srom_vars_len(defaultsromvars_4331);
			bcopy(defaultsromvars_4331, vp, defvarslen);
			vp += defvarslen;
			*vp++ = '\0';
			goto varsdone;
		} else
#endif 
		{
			err = -1;
			goto errout;
		}
	}

varscont:
	/* Bitmask for the sromrev */
	sr = 1 << sromrev;

	/* srom version check: Current valid versions: 1, 2, 3, 4, 5, 8, SROM_MAXREV */
	if ((sr & 0x33e) == 0) {
		err = -2;
		goto errout;
	}

	ASSERT(vars != NULL);
	ASSERT(count != NULL);

	base = vp = MALLOC(osh, MAXSZ_NVRAM_VARS);
	ASSERT(vp != NULL);
	if (!vp) {
		err = -2;
		goto errout;
	}

	/* read variables from flash */
	if (flash) {
		if ((err = initvars_flash(sih, osh, &vp, MAXSZ_NVRAM_VARS)))
			goto errout;
		goto varsdone;
	}

	varbuf_init(&b, base, MAXSZ_NVRAM_VARS);

	/* parse SROM into name=value pairs. */
	_initvars_srom_pci(sromrev, srom, 0, &b);


	/* final nullbyte terminator */
	ASSERT(b.size >= 1);
	vp = b.buf;
	*vp++ = '\0';

	ASSERT((vp - base) <= MAXSZ_NVRAM_VARS);

varsdone:
	err = initvars_table(osh, base, vp, vars, count);

errout:
/* BCMHOSTVARS are enabled only if WLTEST is enabled or BCMEXTNVM is enabled */
#if defined(BCMHOSTVARS)
	if (base && (base != mfgsromvars))
#else
	if (base)
#endif 
		MFREE(osh, base, MAXSZ_NVRAM_VARS);

	MFREE(osh, srom, SROM_MAX);
	return err;
}

/*
 * Read the cis and call parsecis to initialize the vars.
 * Return 0 on success, nonzero on error.
 */
static int
BCMATTACHFN(initvars_cis_pcmcia)(si_t *sih, osl_t *osh, char **vars, uint *count)
{
	uint8 *cis = NULL;
	int rc;
	uint data_sz;

	data_sz = (sih->buscorerev == 1) ? SROM_MAX : CIS_SIZE;

	if ((cis = MALLOC(osh, data_sz)) == NULL)
		return (-2);

	if (sih->buscorerev == 1) {
		if (srom_read(sih, PCMCIA_BUS, (void *)NULL, osh, 0, data_sz, (uint16 *)cis,
		              TRUE)) {
			MFREE(osh, cis, data_sz);
			return (-1);
		}
		/* fix up endianess for 16-bit data vs 8-bit parsing */
		htol16_buf((uint16 *)cis, data_sz);
	} else
		OSL_PCMCIA_READ_ATTR(osh, 0, cis, data_sz);

	rc = srom_parsecis(osh, &cis, SROM_CIS_SINGLE, vars, count);

	MFREE(osh, cis, data_sz);

	return (rc);
}


#ifdef BCMSPI
/*
 * Read the SPI cis and call parsecis to initialize the vars.
 * Return 0 on success, nonzero on error.
 */
static int
BCMATTACHFN(initvars_cis_spi)(osl_t *osh, char **vars, uint *count)
{
	uint8 *cis;
	int rc;

#if defined(NDIS) && !defined(UNDER_CE)
	uint8 cisd[SBSDIO_CIS_SIZE_LIMIT];
	cis = (uint8*)cisd;
#else
	if ((cis = MALLOC(osh, SBSDIO_CIS_SIZE_LIMIT)) == NULL) {
		return -1;
	}
#endif /* defined(NDIS) && (!defined(UNDER_CE)) */

	bzero(cis, SBSDIO_CIS_SIZE_LIMIT);

	if (bcmsdh_cis_read(NULL, SDIO_FUNC_1, cis, SBSDIO_CIS_SIZE_LIMIT) != 0) {
#if defined(NDIS) && !defined(UNDER_CE)
		/* nothing to do */
#else
		MFREE(osh, cis, SBSDIO_CIS_SIZE_LIMIT);
#endif /* defined(NDIS) && (!defined(UNDER_CE)) */
		return -2;
	}

	rc = srom_parsecis(osh, &cis, SDIO_FUNC_1, vars, count);

#if defined(NDIS) && !defined(UNDER_CE)
	/* nothing to do here */
#else
	MFREE(osh, cis, SBSDIO_CIS_SIZE_LIMIT);
#endif

	return (rc);
}
#endif /* BCMSPI */

#if defined(BCMUSBDEV)
/* Return sprom size in 16-bit words */
uint
srom_size(si_t *sih, osl_t *osh)
{
	uint size = 0;
	if (SPROMBUS == PCMCIA_BUS) {
		uint32 origidx;
		sdpcmd_regs_t *pcmregs;
		bool wasup;

		origidx = si_coreidx(sih);
		pcmregs = si_setcore(sih, PCMCIA_CORE_ID, 0);
		if (!pcmregs)
			pcmregs = si_setcore(sih, SDIOD_CORE_ID, 0);
		ASSERT(pcmregs);

		if (!(wasup = si_iscoreup(sih)))
			si_core_reset(sih, 0, 0);

		/* not worry about earlier core revs */
		/* valid for only pcmcia core */
		if (si_coreid(sih) == PCMCIA_CORE_ID)
			if (si_corerev(sih) < 8)
				goto done;


		switch (SI_PCMCIA_READ(osh, pcmregs, SROM_INFO) & SRI_SZ_MASK) {
		case 1:
			size = 256;	/* SROM_INFO == 1 means 4kbit */
			break;
		case 2:
			size = 1024;	/* SROM_INFO == 2 means 16kbit */
			break;
		default:
			break;
		}

	done:
		if (!wasup)
			si_core_disable(sih, 0);

		si_setcoreidx(sih, origidx);
	}
	return size;
}
#endif 

/*
 * initvars are different for BCMUSBDEV and BCMSDIODEV.  This is OK when supporting both at
 * the same time, but only because all of the code is in attach functions and not in ROM.
 */

#if defined(BCMUSBDEV)
#if defined(BCMUSBDEV_BMAC) || defined(BCM_BMAC_VARS_APPEND)
/*
 * Read the USB cis and call parsecis to initialize the vars.
 * Return 0 on success, nonzero on error.
 */
static int
BCMATTACHFN(initvars_cis_usbdriver)(si_t *sih, osl_t *osh, char **vars, uint *count)
{
	uint8 *cis;
	uint sz = OTP_SZ_MAX/2; /* size in words */
	int rc = BCME_OK;

	if ((cis = MALLOC(osh, OTP_SZ_MAX)) == NULL) {
		return -1;
	}

	bzero(cis, OTP_SZ_MAX);

	if (otp_read_region(sih, OTP_SW_RGN, (uint16 *)cis, &sz)) {
		BS_ERROR(("%s: OTP read SW region failure.\n*", __FUNCTION__));
		rc = -2;
	} else {
		BS_ERROR(("%s: OTP programmed. use OTP for srom vars\n*", __FUNCTION__));
		rc = srom_parsecis(osh, &cis, SROM_CIS_SINGLE, vars, count);
	}

	MFREE(osh, cis, OTP_SZ_MAX);

	return (rc);
}

/* For driver(not bootloader), if nvram is not downloadable or missing, use default */
static int
BCMATTACHFN(initvars_srom_si_usbdriver)(si_t *sih, osl_t *osh, char **vars, uint *varsz)
{
	uint len;
	char *base;
	char *fakevars;
	int rc = -1;
	static bool srvars = FALSE; /* Use OTP/SROM as global variables */

	base = fakevars = NULL;
	len = 0;
	switch (CHIPID(sih->chip)) {
		case BCM4322_CHIP_ID:   case BCM43221_CHIP_ID:  case BCM43231_CHIP_ID:
			fakevars = defaultsromvars_4322usb;
			break;
		case BCM43236_CHIP_ID: case BCM43235_CHIP_ID:  case BCM43238_CHIP_ID:
		case BCM43234_CHIP_ID:
			/* check against real chipid instead of compile time flag */
			if (sih->chip == BCM43234_CHIP_ID) {
				fakevars = defaultsromvars_43234usb;
			} else if (sih->chip == BCM43235_CHIP_ID) {
				fakevars = defaultsromvars_43235usb;
			} else
				fakevars = defaultsromvars_43236usb;
			break;

		case BCM4319_CHIP_ID:
			fakevars = defaultsromvars_4319usb;
			break;
		default:
			ASSERT(0);
			return rc;
	}

#ifndef BCM_BMAC_VARS_APPEND
	if (BCME_OK == initvars_cis_usbdriver(sih, osh, vars, varsz)) {
		/* Make OTP/SROM variables global */
		if (srvars == FALSE)
			nvram_append((void *)sih, *vars, *varsz);
		return BCME_OK;
	}
#endif /* BCM_BMAC_VARS_APPEND */

	/* NO OTP, if nvram downloaded, use it */
	if ((_varsz != 0) && (_vars != NULL)) {
		len  = _varsz + (strlen(vstr_end));
		base = MALLOC(osh, len + 2); /* plus 2 terminating \0 */
		if (base == NULL) {
			BS_ERROR(("initvars_srom_si: MALLOC failed.\n"));
			return BCME_ERROR;
		}
		bzero(base, len + 2);

		/* make a copy of the _vars, _vars is at the top of the memory, cannot append 
		 * END\0\0 to it. copy the download vars to base, back of the terminating \0,
		 * then append END\0\0
		 */
		bcopy((void *)_vars, base, _varsz);
		/* backoff all the terminating \0s except the one the for the last string */
		len = _varsz;
		while (!base[len - 1])
			len--;
		len++; /* \0  for the last string */
		/* append END\0\0 to the end */
		bcopy((void *)vstr_end, (base + len), strlen(vstr_end));
		len += (strlen(vstr_end) + 2);
		*vars = base;
		*varsz = len;

		BS_ERROR(("%s USB nvram downloaded %d bytes\n", __FUNCTION__, _varsz));
	} else {
		/* Fall back to fake srom vars if OTP not programmed */
		len = srom_vars_len(fakevars);
		base = MALLOC(osh, (len + 1));
		if (base == NULL) {
			BS_ERROR(("initvars_srom_si: MALLOC failed.\n"));
			return BCME_ERROR;
		}
		bzero(base, (len + 1));
		bcopy(fakevars, base, len);
		*(base + len) = '\0';           /* add final nullbyte terminator */
		*vars = base;
		*varsz = len + 1;
		BS_ERROR(("initvars_srom_usbdriver: faked nvram %d bytes\n", len));
	}

#ifdef BCM_BMAC_VARS_APPEND
	if (BCME_OK == initvars_cis_usbdriver(sih, osh, vars, varsz)) {
		if (base)
			MFREE(osh, base, (len + 1));
	}
#endif	/* BCM_BMAC_VARS_APPEND */
	/* Make OTP/SROM variables global */
	if (srvars == FALSE) {
		nvram_append((void *)sih, *vars, *varsz);
		srvars = TRUE;
	}
	return BCME_OK;

}
#endif /* BCMUSBDEV_BMAC || BCM_BMAC_VARS_APPEND */

#ifdef BCM_DONGLEVARS
static int
BCMATTACHFN(initvars_srom_si_bl)(si_t *sih, osl_t *osh, void *curmap, char **vars, uint *varsz)
{
	int sel = 0;		/* where to read srom/cis: 0 - none, 1 - otp, 2 - sprom */
	uint sz = 0;		/* srom size in bytes */
	void *oh = NULL;
	int rc = BCME_OK;

	if ((oh = otp_init(sih)) != NULL && (otp_status(oh) & OTPS_GUP_SW)) {
		/* Access OTP if it is present, powered on, and programmed */
		sz = otp_size(oh);
		sel = 1;
	} else if ((sz = srom_size(sih, osh)) != 0) {
		/* Access the SPROM if it is present */
		sz <<= 1;
		sel = 2;
	}

	/* Read CIS in OTP/SPROM */
	if (sel != 0) {
		uint16 *srom;
		uint8 *body = NULL;
		uint otpsz = sz;

		ASSERT(sz);

		/* Allocate memory */
		if ((srom = (uint16 *)MALLOC(osh, sz)) == NULL)
			return BCME_NOMEM;

		/* Read CIS */
		switch (sel) {
		case 1:
			rc = otp_read_region(sih, OTP_SW_RGN, srom, &otpsz);
			sz = otpsz;
			body = (uint8 *)srom;
			break;
		case 2:
			rc = srom_read(sih, SI_BUS, curmap, osh, 0, sz, srom, TRUE);
			/* sprom has 8 byte h/w header */
			body = (uint8 *)srom + SBSDIO_SPROM_CIS_OFFSET;
			break;
		default:
			/* impossible to come here */
			ASSERT(0);
			break;
		}

		/* Parse CIS */
		if (rc == BCME_OK) {
			/* each word is in host endian */
			htol16_buf((uint8 *)srom, sz);
			ASSERT(body);
			rc = srom_parsecis(osh, &body, SROM_CIS_SINGLE, vars, varsz);
		}

		MFREE(osh, srom, sz);	/* Clean up */

		/* Make SROM variables global */
		if (rc == BCME_OK)
			nvram_append((void *)sih, *vars, *varsz);
	}

	return BCME_OK;
}
#endif	/* #ifdef BCM_DONGLEVARS */

static int
BCMATTACHFN(initvars_srom_si)(si_t *sih, osl_t *osh, void *curmap, char **vars, uint *varsz)
{
	static bool srvars = FALSE;	/* Use OTP/SPROM as global variables */

	/* Bail out if we've dealt with OTP/SPROM before! */
	if (srvars)
		goto exit;

#if defined(BCMUSBDEV_BMAC) || defined(BCM_BMAC_VARS_APPEND)
	/* read OTP or use faked var array */
	switch (CHIPID(sih->chip)) {
		case BCM4322_CHIP_ID:   case BCM43221_CHIP_ID:  case BCM43231_CHIP_ID:
		case BCM43236_CHIP_ID:  case BCM43235_CHIP_ID:  case BCM43238_CHIP_ID:
		case BCM43234_CHIP_ID:
		case BCM4319_CHIP_ID:
		if (BCME_OK != initvars_srom_si_usbdriver(sih, osh, vars, varsz))
			goto exit;
		return BCME_OK;
		default:
			UNUSED_PARAMETER(defaultsromvars_4322usb);
			UNUSED_PARAMETER(defaultsromvars_43234usb);
			UNUSED_PARAMETER(defaultsromvars_43235usb);
			UNUSED_PARAMETER(defaultsromvars_43236usb);
			UNUSED_PARAMETER(defaultsromvars_4319usb);
	}
#endif  /* BCMUSBDEV_BMAC || BCM_BMAC_VARS_APPEND */

#ifdef BCM_DONGLEVARS	    /* this flag should be defined for usb bootloader, to read \
	OTP or SROM */
	if (BCME_OK != initvars_srom_si_bl(sih, osh, curmap, vars, varsz))
		return BCME_ERROR;
#endif

	/* update static local var to skip for next call */
	srvars = TRUE;

exit:
	/* Tell the caller there is no individual SROM variables */
	*vars = NULL;
	*varsz = 0;

	/* return OK so the driver will load & use defaults if bad srom/otp */
	return BCME_OK;
}

#else /* !BCMUSBDEV && !BCMSDIODEV */

static int
BCMATTACHFN(initvars_srom_si)(si_t *sih, osl_t *osh, void *curmap, char **vars, uint *varsz)
{
	/* Search flash nvram section for srom variables */
	return initvars_flash_si(sih, vars, varsz);
}
#endif	
