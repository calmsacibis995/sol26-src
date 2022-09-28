#
#ident	"@(#)Makefile	1.15	96/10/10 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
#
# lib/libvolmgt/Makefile
#
LIBRARY= libvolmgt.a
VERS=.1

LOC_OBJS= volattr.o volutil.o volprivate.o volname.o volmgt_fsi.o \
	volmgt_fsidbi.o volmgt_on_private.o

OBJECTS= $(LOC_OBJS)

# include library definitions
include ../Makefile.lib

MAPFILE=	mapfile-vers

# hack SRCS to work for our reg stuff (overriding what is in ../Makefile.lib)
SRCS=$(LOC_OBJS:.o=.c)

LIBS += $(DYNLIB)

lint :=	LINTFLAGS=-ua -errchk=longptr64 -errhdr -fd -Ncheck -Nlevel=4

CLEANFILES=	$(LINTOUT) $(LINTLIB)

LDLIBS +=       -ladm -lc
DYNFLAGS +=	-M $(MAPFILE)

# definitions for install_h target
HDRS=		volmgt.h
ROOTHDRDIR=	$(ROOT)/usr/include
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=	$(HDRS:%.h=%.check)

$(ROOTHDRS) :=	FILEMODE= 644

# install rule for install_h target
$(ROOTHDRDIR)/%: %
	$(INS.file)

.KEEP_STATE:

all: $(LIBS) $(TXTS)

$(DYNLIB): $(MAPFILE)

install_h: $(ROOTHDRS)

check: $(CHECKHDRS)

install: all $(ROOTLIBS) $(ROOTLINKS)


lint:	$(LINTLIB)

# include library targets
include ../Makefile.targ
