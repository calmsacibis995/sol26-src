#
#ident	"@(#)Makefile	1.11	96/09/18 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# ucblib/librpcsoc/Makefile
#
LIBRARY= librpcsoc.a
VERS = .1

OBJECTS= clnt_tcp.o clnt_udp.o getrpcport.o rtime.o svc_tcp.o svc_udp.o get_myaddress.o

# include library definitions
include ../../lib/Makefile.lib

MAPFILE=	mapfile-vers

LDLIBS += -lnsl -lsocket -lc
CFLAGS += -DPORTMAP
DYNFLAGS += -M $(MAPFILE)

ROOTLIBDIR=	$(ROOT)/usr/ucblib

LINTSRC= $(LINTLIB:%.ln=%)
ROOTLINTDIR= $(ROOTLIBDIR)
ROOTLINT= $(LINTSRC:%=$(ROOTLINTDIR)/%)

# install rule for lint library target
$(ROOTLINTDIR)/%: %
	$(INS.file)

# install rule for install_h target
$(ROOTHDRDIR)/%: %
	$(INS.file)

LIBS = $(DYNLIB) $(LINTLIB)

$(DYNLIB):	$(MAPFILE)

.KEEP_STATE:

all: $(LIBS)

$(LINTLIB):= SRCS=llib-lrpcsoc
$(LINTLIB):= LINTFLAGS=-nvx
$(LINTLIB):= TARGET_ARCH=

install: $(ROOTLIBDIR) $(ROOTLIBS) $(ROOTLINT) $(ROOTLINKS)

install_h: $(ROOTHDRS)

# include library targets
include ../../lib/Makefile.targ
include ../Makefile.ucbtarg
