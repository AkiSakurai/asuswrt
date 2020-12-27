.PHONY: all clean

SRCPATH = ../source
HDRPATH = ../include
INFPATH = ../interface
XMLPATH = $(INFPATH)/xml
SQLPATH = $(INFPATH)/sqlite
WARPATH = ../war
LINUXPATH = $(WARPATH)/linux
LINUXINCLUDE = $(LINUXPATH)/include

BUILDPATH = ../build
OBJPATH = $(BUILDPATH)/obj
BINPATH = $(BUILDPATH)/bin

CC = gcc
CFLAGS = -O -Wall
#------------ Complier Options ------------
#Enable detailed log: CODE_DEBUG
#Enable IPv6: __DEVICE_IPV6__
#Enable WiMAX WIB Bootstrap: TR069_WIB
#Enable Femto PM and CM: TR196
#Enable SSL: __ENABLE_SSL__
#Enable TR-157 diagnostics: TR157
#Enable TR-143 diagnostics: TR143
#Backward Compatibility to v4.2: __V4_2
#Support VT100 ANSI Terminal Color for log: VT100_COMPAT
#Support STUN client: TR111
#Support TR-181 data model: TR181
#Support TR-106 data model: TR106
#Support alias based addressing: ALIAS
#------------------------------------------
#DEFS = -DCODE_DEBUG -DVT100_COMPAT -DUSE_C99 -D__ENABLE_SSL__ -DWITH_STRPTIME -D__DEVICE_IPV6__
#DEFS = -DVT100_COMPAT -DUSE_C99 -DCODE_DEBUG -D__ENABLE_SSL__ -DTR069_WIB -DTR143 -DTR111 -DTR196 -D__V4_2
DEFS = -DVT100_COMPAT -DUSE_C99 -DCODE_DEBUG -D__ENABLE_SSL__ -DTR143 -DTR111 -DTR157 -DTR106 -DTR181 -DALIAS
LIBS = -lssl -lpthread -lsqlite3

INCLUDES = -I$(HDRPATH) -I$(LINUXINCLUDE) -I$(XMLPATH)
#INCLUDES = -I$(HDRPATH) -I$(LINUXINCLUDE) -I$(SQLPATH)

VPATH = $(SRCPATH):$(XMLPATH):$(LINUXPATH)/string:$(LINUXPATH)/math:$(LINUXPATH)/file:$(LINUXPATH)/socket:$(LINUXPATH)/time:$(LINUXPATH)/error:$(LINUXPATH)/log
#VPATH = $(SRCPATH):$(SQLPATH):$(LINUXPATH)/string:$(LINUXPATH)/math:$(LINUXPATH)/file:$(LINUXPATH)/socket:$(LINUXPATH)/time:$(LINUXPATH)/error:$(LINUXPATH)/log

SRCS=$(wildcard $(SRCPATH)/*.c) $(wildcard $(XMLPATH)/*.c)  $(wildcard $(LINUXPATH)/*/*.c)
#SRCS=$(wildcard $(SRCPATH)/*.c) $(wildcard $(SQLPATH)/*.c) $(wildcard $(LINUXPATH)/*/*.c)

HEADERS=$(wildcard $(HDRPATH)/*.h)
NOTDIRSRCS=$(notdir $(SRCS))
OBJS = $(patsubst %.c,$(OBJPATH)/%.o,$(NOTDIRSRCS))

all: tr

tr: $(OBJS)
	@mkdir -p $(BINPATH)
	$(CC) -s -o $(BINPATH)/$@ $^ $(LIBS)
	@echo Compile OneAgent-TR successful....

$(OBJS): $(OBJPATH)/%.o: %.c
	@mkdir -p $(OBJPATH)
	$(CC) $(CFLAGS) -c -o $@  $< $(INCLUDES) $(DEFS)

clean:
	rm -rf $(BINPATH)/* $(OBJPATH)/* 
