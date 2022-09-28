#
#ident	"@(#)Makefile	1.8	96/08/29 SMI"
#
# Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/context/onc_ns/nis/Makefile
#

LIBRARYCCC= fn_ctx_onc_fn_nis.a
VERS = .1

DEFOBJS = \
	FNSP_nisOrgContext_default.o \
	FNSP_nisFlatContext_default.o

CTXOBJS = \
	FNSP_nisFlatContext.o FNSP_nisHUContext.o \
	FNSP_nisWeakSlashContext.o \
	FNSP_nisDotContext.o FNSP_nisHierContext.o \
	FNSP_nisPrinternameContext.o FNSP_nisPrinterObject.o \
	FNSP_nisOrgContext.o 

OBJECTS = $(CTXOBJS) $(DEFOBJS) \
	FNSP_nisAddress.o \
	FNSP_nisImpl.o fnsp_nis_internal.o fnsp_internal_common.o \
	onc_fn_nis.o FNSP_hash_function.o

FNLINKS= fn_ctx_onc_fn_printer_nis.so$(VERS) fn_ctx_onc_fn_nis_root.so$(VERS)

# include library definitions
include ../../../Makefile.libfn

FNRPATH=	/usr/lib/fn
FNSPINCDIR=	$(SRC)/lib/fn/context/onc_ns/libfnsp
CPPFLAGS +=	-I$(FNSPINCDIR)

LIBS = $(DYNLIBCCC)
FNSPLIB= -L$(FNLIBDIR) -lfnsp

LDLIBS += -lxfn -lfn_spf $(FNSPLIB) -lnsl -lfn_p -ldl -lC -lc

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
