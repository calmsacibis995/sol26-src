#
#ident	"@(#)Makefile	1.14	96/07/08 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/nsswitch/nisplus/Makefile
#
LIBRARY= libnss_nisplus.a
VERS= .1

OBJECTS= nisplus_common.o \
		gethostent.o getrpcent.o \
		getnetent.o getprotoent.o getservent.o \
		getpwnam.o getgrent.o getspent.o \
		bootparams_getbyname.o ether_addr.o \
		getnetgrent.o netmasks.o

# include library definitions, do not change order of include and DYNLIB
include ../../Makefile.lib

MAPFILE=	mapfile-vers

DYNLIB=	nss_nisplus.so$(VERS)

LINTFLAGS=
CPPFLAGS += -D_REENTRANT
LDLIBS += -lnsl -lsocket -lc
DYNFLAGS +=	-M $(MAPFILE)

LIBS += $(DYNLIB)

.KEEP_STATE:

all: $(LIBS)

$(DYNLIB):	$(MAPFILE)

install: all $(ROOTLIBS)

# include library targets
include ../../Makefile.targ
