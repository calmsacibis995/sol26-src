#
#ident "@(#)Makefile  1.5     96/08/26 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# lib/libposix/Makefile
#
LIBRARY =	libposix4.a
VERS =		.1
INCLIST=	-Iinc

# include common library definitions
include ../Makefile.lib

CPPFLAGS=	$(INCLIST) $(CPPFLAGS.master) -D_REENTRANT
LIBS =		$(DYNLIB)

#HDRS = 
ROOTHDRDIR =	$(ROOT)/usr/include/sys
ROOTHDRS =	$(HDRS:%=$(ROOTHDRDIR)/%)

$(ROOTHDRS) :=	FILEMODE = 644

.KEEP_STATE:

all:		$(MACHLIBS)

install:	$(ROOTLIBS) $(ROOTLINKS)

$(ROOTHDRDIR)/% : %
		$(INS.file)

install_h: $(ROOTHDRS)

clean clobber:	FRC
		@cd $(MACH); pwd; $(MAKE) $@

links:
		-$(RM) $(MACH)
		$(SYMLINK) `mach` $(MACH)
include ../Makefile.mach

FRC:
