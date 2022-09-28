#
#ident	"@(#)Makefile	1.4	96/09/18 SMI"
#
# Copyright (c) 1994, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/liblm/Makefile
#
LIBRARY= liblm.a
VERS=.1

OBJECTS= lm_shutdown.o

# include library definitions
include ../Makefile.lib

MAPFILES=	mapfile-vers
MAPOPTS=	$(MAPFILES:%=-M %)

$(DYNLIB):	$(MAPFILES)
LIBS = $(DYNLIB)

LINTFLAGS=	-u
LINTOUT=	lint.out
CLEANFILES=	$(LINTOUT) $(LINTLIB)

LDLIBS +=       -lelf -lc

DYNFLAGS +=	$(MAPOPTS)

.KEEP_STATE:

all: $(LIBS) $(TXTS)

install: all $(ROOTLIBS) $(ROOTLINKS)

lint:	$(LINTLIB)

# include library targets
include ../Makefile.targ
