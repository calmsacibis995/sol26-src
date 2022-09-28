#
#ident	"@(#)Makefile 1.1	96/05/20 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
LIBRARY= libsched.a
VERS = .1
INCLIST= -Iinc

# include common library definitions
include ../Makefile.lib

CPPFLAGS= $(INCLIST) $(CPPFLAGS.master)
LIBS = $(DYNLIB)

ROOTHDRDIR = $(ROOT)/usr/include/sys
ROOTHDRS = $(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS = $(HDRS:%.h=%.check)

$(ROOTHDRS) :=	FILEMODE = 644

.KEEP_STATE:

all: $(MACHLIBS)

install: $(ROOTLIBS) $(ROOTLINKS)

$(ROOTHDRDIR)/% : %
	$(INS.file)

install_h: $(ROOTHDRS)

check:	$(CHECKHDRS)

clean clobber: FRC
	@cd $(MACH); pwd; $(MAKE) $@

links:
	-$(RM) $(MACH)
	$(SYMLINK) `mach` $(MACH)
include ../Makefile.mach

FRC:
