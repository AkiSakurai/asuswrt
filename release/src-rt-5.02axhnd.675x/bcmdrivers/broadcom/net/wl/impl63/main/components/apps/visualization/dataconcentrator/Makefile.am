# <<Broadcom-WL-IPTag/Proprietary:>>

AUTOMAKE_OPTIONS = foreign

VISSHARED=../shared
SRCDIR = $(BASEDIR)/src

CFLAGS=-Wall -Werror -I$(SRCDIR)/common/include/ -I$(SRCDIR)/include/ -I../include/ \
		-I$(SRCDIR)/../components/opensource/jsonc/ \
		-I$(SRCDIR)/../components/opensource/libxml2/include/ \
		-I$(SRCDIR)/../components/opensource/sqlite3/ \
		-I$(SRCDIR)/../components/shared/ \
	  -I$(SRCDIR)/../components/proto/include/ \
	  -I$(SRCDIR)/../components/wlioctl/include/ \
		-DVERSION=\"$(VERSION)\" -DPROG="\"$(PACKAGE)\""

LDFLAGS=-L$(SRCDIR)/../components/opensource/sqlite3/installbin/lib \
		-L$(SRCDIR)/../components/opensource/jsonc/installbin/lib \
		-L$(SRCDIR)/../components/opensource/libxml2/installbin/lib \
		-lsqlite3 -lpthread -lxml2 -ljson-c -L$(SRCBASE_ROUTER)/nvram -L$(INSTALLDIR)/nvram/usr/lib -lnvram
LDFLAGS	+= $(EXTRA_LDFLAGS)
if ISBCMINTERNAL
CFLAGS  += -DBCMINTERNAL
endif
CFLAGS  += -DBCMDBG

bin_PROGRAMS = vis-dcon
vis_dcon_SOURCES = $(VISSHARED)/vis_xmlshared.c $(VISSHARED)/vis_sock_util.c \
			$(VISSHARED)/vis_shared_util.c vis_synchdb.c \
			vis_utility.c database.c vis_xmlutility.c vis_jsonutility.c \
			vis_sockserver.c vis_dcon_main.c

clean-local:
	rm -f Makefile
	rm -rf autom4te.cache
	rm -f config.h
	rm -f config.log
	rm -f config.status
	rm -f stamp-h1
	rm -rf installbin
