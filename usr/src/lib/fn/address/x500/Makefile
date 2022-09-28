#
#ident	"@(#)Makefile	1.3	96/03/31 SMI"
#
# Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/address/x500/Makefile
#

LIBRARYCCC= fn_ref_addr_x500.a
VERS = .1

OBJECTS= x500.o

# include library definitions
include ../../Makefile.libfn


LIBS = $(DYNLIBCCC)

LDLIBS += -lxfn -lC -lc

.KEEP_STATE:

all: $(LIBS) 

debug install tcov: $(ROOTLIBDIR) $(ROOTLIBS)

install_h:

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

# include library targets
include ../../Makefile.targ
