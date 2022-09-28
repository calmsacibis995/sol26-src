#
#ident	"@(#)Makefile	1.9	95/03/09 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# lib/nsswitch/compat/Makefile
#
LIBRARY= libnss_compat.a
VERS= .1

OBJECTS= getpwent.o getgrent.o getspent.o compat_common.o

# include library definitions, do not change the order of include and DYNLIB
include ../../Makefile.lib

MAPFILE=	mapfile-vers

DYNLIB= nss_compat.so$(VERS)

LINTFLAGS=
CPPFLAGS += -D_REENTRANT
LDLIBS += -lnsl -lc
DYNFLAGS += -M $(MAPFILE)

LIBS += $(DYNLIB)

.KEEP_STATE:

all: $(LIBS)

$(DYNLIB):	$(MAPFILE)

install: all $(ROOTLIBS)

# include library targets
include ../../Makefile.targ
