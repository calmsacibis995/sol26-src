#
#ident	"@(#)Makefile	1.3	96/03/31 SMI"
#
# Copyright (c) 1989 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/address/fn_link_addr/Makefile
#

LIBRARYCCC= fn_ref_addr_fn_link_addr.a
VERS = .1

OBJECTS= fn_link_addr.o

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
