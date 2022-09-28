#
#ident	"@(#)Makefile	1.3	96/03/31 SMI"
#
# Copyright (c) 1989 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/address/onc_fn_printer_files/Makefile
#

LIBRARYCCC= fn_ref_addr_onc_fn_printer_files.a
VERS = .1

OBJECTS= onc_fn_printer_files.o

# include library definitions
include ../../Makefile.libfn

LIBS = $(DYNLIBCCC)

LDLIBS += -lxfn -lfn_p -lC -lc

.KEEP_STATE:

all: $(LIBS) 

debug install tcov: $(ROOTLIBDIR) $(ROOTLIBS)

install_h:

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

# include library targets
include ../../Makefile.targ
