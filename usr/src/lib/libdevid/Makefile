#
#pragma ident   "@(#)Makefile 1.4     96/09/18 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#

LIBRARY=	libdevid.a
VERS=		.1
OBJECTS=	deviceid.o

# include library definitions
include ../Makefile.lib

MAPFILE=	mapfile-vers
ROOTLIBDIR= $(ROOT)/usr/lib
LIBS =	$(DYNLIB)

# definitions for install_h target
HDRS=		libdevid.h
ROOTHDRDIR=	$(ROOT)/usr/include
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=	$(HDRS:%.h=%.check)

DYNFLAGS += -M $(MAPFILE)

LINTOUT=	lint.out
CLEANFILES=	$(LINTOUT) $(LINTLIB)

$(DYNLIB): $(MAPFILE)
LDLIBS += -lc

.KEEP_STATE:

all: 		$(LIBS)

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
