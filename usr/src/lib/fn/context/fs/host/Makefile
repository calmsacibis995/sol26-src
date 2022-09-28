#
#pragma ident	"@(#)Makefile	1.2	96/03/31 SMI"
#
# Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/context/fs/host/Makefile
#

LIBRARYCCC= fn_ctx_onc_fn_fs_host.a
VERS = .1

OBJECTS = onc_fn_fs_host.o FSHost.o Export.o getexports.o xdr_utils.o

# include library definitions
include ../../../Makefile.libfn

LIBS = $(DYNLIBCCC)

CPPFLAGS += -I$(SRC)/lib/fn/libxfn

LDLIBS += -lxfn -lfn_spf -lrpcsvc -lnsl -lC -lc

all: $(LIBS) 

debug install tcov: $(ROOTLIBDIR) $(ROOTLIBS)

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

# include library targets
include ../../../Makefile.targ

.KEEP_STATE:
