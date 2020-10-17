# <<Broadcom-WL-IPTag/Proprietary:>>

AUTOMAKE_OPTIONS = foreign

SRCDIR = $(BASEDIR)/src
VISSHARED=../shared
BCMWIFIDIR=$(SRCDIR)/shared/bcmwifi/src
SHAREDDIR=$(SRCDIR)/shared
COMPONENTSDIR = $(SRCDIR)/../components
MATHDIR = $(COMPONENTSDIR)/math/src

CFLAGS=-Wall -Werror -I$(SRCDIR)/shared/bcmwifi/include/ \
	-I$(SRCDIR)/common/include/ \
	-I$(SRCDIR)/include/ -I../include/ \
	-I$(SRCDIR)/../components/opensource/libxml2/include/ \
	-I$(SRCDIR)/../components/shared/ \
	-I$(SRCDIR)/../components/wlioctl/include/ \
	-I$(SRCDIR)/../components/math/include/ \
	-I$(SRCDIR)/../components/proto/include/ \
	-I$(SRCDIR)/router/shared/ \
	-I$(SRCDIR)/../components/router/shared/ \
	-DVERSION=\"$(VERSION)\" -DPROG="\"$(PACKAGE)\""

LDFLAGS=-L$(SRCDIR)/../components/opensource/libxml2/installbin/lib \
		-lxml2 -lpthread -L$(SRCBASE_ROUTER)/nvram -L$(INSTALLDIR)/nvram/usr/lib -lnvram -lm

LDFLAGS	+= $(EXTRA_LDFLAGS)

if ISBCMINTERNAL
CFLAGS  += -DBCMINTERNAL
endif
CFLAGS  += -DBCMDBG

bin_PROGRAMS = vis-datacollector
vis_datacollector_SOURCES = $(SHAREDDIR)/bcmutils.c \
			$(MATHDIR)/bcm_math.c \
			$(SHAREDDIR)/bcmxtlv.c $(SHAREDDIR)/bcm_app_utils.c \
			$(BCMWIFIDIR)/bcmwifi_channels.c vis_common.c \
			$(VISSHARED)/vis_xmlshared.c $(VISSHARED)/vis_sock_util.c \
			$(VISSHARED)/vis_shared_util.c \
			vis_socketclient.c vis_xmlutility.c vis_savedata.c\
			vis_wl.c vis_wlmetrics.c vis_wlcongestion.c  vis_rrmprocess.c \
			vis_linux_main.c

clean-local:
	rm -f Makefile
	rm -rf autom4te.cache
	rm -f config.h
	rm -f config.log
	rm -f config.status
	rm -f stamp-h1
	rm -rf installbin
