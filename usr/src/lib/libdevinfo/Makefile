#
#pragma ident	"@(#)Makefile	1.10	96/07/29 SMI"
#
# Copyright (c) 1990 - 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/devlinks/libdevfs/Makefile
#
LIBRARY=	libdevinfo.a
VERS=		.1

OBJECTS=	devfswalk.o devfssubr.o devfsinfo.o

# include library definitions
include ../Makefile.lib

MAPFILES=	mapfile-vers
MAPOPTS=	$(MAPFILES:%=-M %)

LIBS +=	$(DYNLIB)

LDLIBS += -lc

# definitions for install_h target
HDRS=		device_info.h
ROOTHDRDIR=	$(ROOT)/usr/include
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=	$(HDRS:%.h=%.check)

# install rule for install_h target
$(ROOTHDRDIR)/%: %
	$(INS.file)

CPPFLAGS +=	-v
DYNFLAGS=	-h $(DYNLIB) -ztext $(MAPOPTS)

LINTOUT=	lint.out
CLEANFILES=	$(LINTOUT) $(LINTLIB)

.KEEP_STATE:

all: 		$(LIBS)

install: all $(ROOTLIBS) $(ROOTLINKS)

install_h: $(ROOTHDRS)

check: $(CHECKHDRS)

lint:		$(LINTLIB)

# include library targets
include ../Makefile.targ
