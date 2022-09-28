#
#ident	"@(#)Makefile	1.8	96/09/18 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# lib/nsswitch/dns/Makefile
#
LIBRARY= libnss_dns.a
VERS= .1

OBJECTS= gethostent.o

# include library definitions, do not change order of include and DYNLIB
include ../../Makefile.lib

MAPFILE=	mapfile-vers

DYNLIB=	nss_dns.so$(VERS)

LINTFLAGS=
LDLIBS += -lnsl -lresolv -lc
CPPFLAGS += -D_REENTRANT -DSYSV
DYNFLAGS += -M $(MAPFILE)

LIBS += $(DYNLIB)

.KEEP_STATE:

all: $(LIBS)

$(DYNLIB):	$(MAPFILE)

install: $(ROOTLIBS)

# include library targets
include ../../Makefile.targ
