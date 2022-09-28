#
#ident	"@(#)Makefile	1.2	96/04/17 SMI"
#
# Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/address/onc_fn_nis_root/Makefile
#
#
# produces fn_inet_nis.so$(VERS) (a symbolic link) as well

LIBRARYCCC= fn_ref_addr_onc_fn_nis_root.a
VERS = .1

OBJECTS= to_ref.o description.o

FNLINKS= fn_inet_nis.so$(VERS)

# include library definitions
include ../../Makefile.libfn


LIBS = $(DYNLIBCCC)

LDLIBS += -lxfn -lfn_p -lnsl -lC -lc 

.KEEP_STATE:

all: $(LIBS) 

install debug tcov: $(ROOTLIBDIR) $(ROOTLIBS) $(ROOTFNLINKS)

install_h:

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

# include library targets
include ../../Makefile.targ
