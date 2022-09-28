#
#ident	"@(#)Makefile	1.17	96/09/18 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc. All rights Reserved.
#
# lib/libdhcp/Makefile
#
LIBRARY= libdhcp.a
VERS= .2
HDRS= dhcdata.h
ROOTHDRDIR=	$(ROOT)/usr/include
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKDIRS=	$(HDRS:%.h=%.check)

OBJECTS= dd.o dd_impl.o dd_defs.o octet.o ufs_dd.o nisplus_dd.o valid.o

# include library definitions
include ../Makefile.lib

MAPFILE=	mapfile-vers
CPPFLAGS += -I.
LDLIBS += -lxfn -lnsl -lc
DYNFLAGS +=	-M $(MAPFILE)
$(ROOTHDRS) :=  FILEMODE= 644

.KEEP_STATE:

all: $(DYNLIB)

$(DYNLIB):	$(MAPFILE)

install: all $(ROOTDYNLIB) $(ROOTLINKS)

install_h: $(ROOTHDRS)

check: $(CHECKHDRS)

lint: llib-ldhcp.ln

$(ROOTHDRDIR)/%: %
	$(INS.file)

# include library targets
include ../Makefile.targ
