#
# Copyright (c) 1992 by Sun Microsystems, Inc.
#
#pragma	ident	"@(#)Makefile	1.7	96/09/18 SMI"
#
# lib/libkstat/Makefile
#

LIBRARY= libkstat.a
VERS= .1
OBJECTS= kstat.o

# include library definitions
include ../Makefile.lib

MAPFILE=	mapfile-vers

ROOTLIBDIR= $(ROOT)/usr/lib
LIBS = $(DYNLIB) # $(LINTLIB) maybe

# definitions for install_h target
HDRS=		kstat.h
ROOTHDRDIR=	$(ROOT)/usr/include
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=	$(HDRS:%.h=%.check)

# install rule for install_h target
$(ROOTHDRDIR)/%: %
	$(INS.file)

DYNFLAGS += -M $(MAPFILE)
LDLIBS += -lc

# definitions for lint
CLEANFILES += $(LINTOUT)

$(DYNLIB): $(MAPFILE)

.KEEP_STATE:

all: $(LIBS)

install: all $(ROOTLIBS) $(ROOTLINKS)

install_h: $(ROOTHDRS)

check:	$(CHECKHDRS)

lint: $(LINTLIB)

# include library targets
include ../Makefile.targ
