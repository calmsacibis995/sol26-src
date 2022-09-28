#
#ident	"@(#)Makefile	1.9	96/09/10 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/swap/Makefile

PROG= swap

include ../Makefile.cmd

CPPFLAGS += -D_LARGEFILE64_SOURCE

FILEMODE= 02555
GROUP= sys

ROOTSYMLINK= $(ROOTETCPROG)

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTUSRSBINPROG) $(ROOTSYMLINK)

$(ROOTSYMLINK):
	$(RM) $@; $(SYMLINK) ../usr/sbin/$(PROG) $@

clean:

lint:	lint_PROG

include ../Makefile.targ
