#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident   "@(#)Makefile 1.2     96/09/29 SMI"
#
# lib/libdevice/Makefile

LIBRARY=	libdevice.a
VERS=		.1
OBJECTS=	devctl.o

# include library definitions
include ../Makefile.lib

MAPFILE=	mapfile-vers
ROOTLIBDIR= $(ROOT)/usr/lib
LIBS =	$(DYNLIB)

# definitions for install_h target
HDRS=		libdevice.h
ROOTHDRDIR=	$(ROOT)/usr/include
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=	$(HDRS:%.h=%.check)

DYNFLAGS += -M $(MAPFILE)
LDLIBS += -lc

CPPFLAGS +=	-v -D_REENTRANT

LINTOUT=	lint.out
CLEANFILES=	$(LINTOUT) $(LINTLIB)

$(DYNLIB): $(MAPFILE)

.KEEP_STATE:

all: $(LIBS)

install: all $(ROOTLIBS) $(ROOTLINKS)

#
# Need this empty target.
# It is called from above
#
install_h:

check: $(CHECKHDRS)

lint: $(LINTLIB)

# include library targets
include ../Makefile.targ
