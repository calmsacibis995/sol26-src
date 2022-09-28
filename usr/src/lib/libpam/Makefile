#
# Copyright (c) 1992-1995, by Sun Microsystems, Inc.
# All rights reserved.
#
# ident "@(#)Makefile	1.7	96/06/10 SMI"
#
# lib/libpam/Makefile
#
LIBRARY= libpam.a
VERS = .1

OBJECTS= pam_framework.o pam_framework_utils.o

# include library definitions
include ../Makefile.lib

#override INS.liblink
INS.liblink=	-$(RM) $@; $(SYMLINK) $(LIBLINKS)$(VERS) $@

#threads
CPPFLAGS += -DOPT_INCLUDE_XTHREADS_H -DSVR4

BUILD.SO=	$(CC) -o $@ -M mapfile -G $(DYNFLAGS) $(PICS) $(LDLIBS)

INS.conf=	$(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(@D) $<

# install pam.conf
ETCFILES= pam.conf
ROOTETC= $(ROOT)/etc
IETCFILES= $(ETCFILES:%=$(ROOTETC)/%)
$(IETCFILES) := FILEMODE = 644
$(IETCFILES) := OWNER = root
$(IETCFILES) := GROUP = sys

HDRS= pam_appl.h pam_modules.h

ROOTDIRS= $(ROOT)/usr/include/security

ROOTHDRS= $(HDRS:%=$(ROOTDIRS)/%)
$(ROOTHDRS) := FILEMODE = 644

CHECKHDRS= $(HDRS:%.h=%.check)

#link rule
$(ROOTLINKS)/%: %
	$(INS.liblink)

#install rule
$(ROOTDIRS)/%: %
	$(INS.file)

# library dependency
LDLIBS += -ldl -lc

LIBS += $(DYNLIB)

.KEEP_STATE:

# include library targets
include ../Makefile.targ

all:  install

install: install_h $(ROOTLIBS) $(ROOTLINKS) $(IETCFILES)

install_h: $(ROOTHDRS)

$(ROOTDIRS):
	$(INS.dir)

$(ROOTETC)/%: %
	$(INS.conf)

lint: llib-lpam.ln

#check: $(CHECKHDRS)

#additional dependencies

$(LIBRARY) : $(OBJS)
$(DYNLIB) : $(PICS)

FRC:
