#
#ident	"@(#)Makefile	1.12	96/07/08 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/nsswitch/nis/Makefile
#
LIBRARY= libnss_nis.a
VERS= .1

OBJECTS= nis_common.o \
	getgrent.o getpwnam.o getspent.o \
	gethostent.o getrpcent.o \
	getservent.o getnetent.o getprotoent.o \
	bootparams_getbyname.o ether_addr.o \
	getnetgrent.o netmasks.o

# include library definitions, do not change the order of include and DYNLIB
include ../../Makefile.lib

MAPFILE=	mapfile-vers

DYNLIB= nss_nis.so$(VERS)

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
