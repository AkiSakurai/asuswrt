#
# Broadstream iqos GUI library Makefile
#
# $Copyright Broadcom Corporation$
#
# $Id$
#

INSTALL_DIR := ../../lib
OBJS := ioc_common.o conf_app.o
TARGET := libiqos_interface.a

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(AR) -cr $(INSTALL_DIR)/$@ $^

clean :
	rm -f $(OBJS) $(INSTALL_DIR)/$(TARGET)
