#
#ident	"@(#)Makefile	1.11	96/03/31 SMI"
#
# Copyright (c) 1989 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/context/onc_ns/nisplus/Makefile
#

LIBRARYCCC= fn_ctx_onc_fn_nisplus.a
VERS = .1

CTXOBJS = FNSP_nisplusWeakSlashContext.o FNSP_nisplusHierContext.o \
	FNSP_nisplusFlatContext.o FNSP_nisplusDotContext.o \
	FNSP_HUContext.o FNSP_nisplusOrgContext.o \
	FNSP_HostnameContext.o FNSP_UsernameContext.o \
	FNSP_ENSContext.o \
	FNSP_nisplusPrinternameContext.o FNSP_nisplusPrinterObject.o

OBJECTS = fnsp_internal.o fnsp_nisplus_root.o onc_fn_nisplus.o \
	fnsp_search.o fnsp_hostuser.o fnsp_attrs.o \
	FNSP_nisplus_address.o $(CTXOBJS) 

FNLINKS= fn_ctx_onc_fn_printer_nisplus.so$(VERS)

# include library definitions
include ../../../Makefile.libfn

FNRPATH=	/usr/lib/fn
FNSPINCDIR=	$(SRC)/lib/fn/context/onc_ns/libfnsp
CPPFLAGS +=	-I$(FNSPINCDIR)

LIBS = $(DYNLIBCCC)
FNSPLIB= -L$(FNLIBDIR) -lfnsp

LDLIBS += -lxfn -lfn_spf $(FNSPLIB) -lnsl -lfn_p -lC -lc

debug :=	CPPFLAGS += -g
tcov :=		CPPFLAGS += -a

.KEEP_STATE:

all: $(LIBS) 

debug install tcov: $(ROOTLIBDIR) $(ROOTLIBS) $(ROOTFNLINKS)

install_h:

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

# include library targets
include ../../../Makefile.targ
