#
#ident	"@(#)Makefile	1.3	96/04/08 SMI"
#
# Copyright (c) 1989 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/context/onc_ns/libfnsp/Makefile
#

LIBRARYCCC= libfnsp.a
VERS = .1

CTXOBJS = FNSP_FlatContext.o  FNSP_HierContext.o \
	FNSP_WeakSlashContext.o \
	FNSP_PrinterContext.o \
	FNSP_PrinternameContext.o FNSP_PrinterObject.o \
	FNSP_defaultContext.o FNSP_OrgContext.o 

COMMONOBJS = FNSP_Syntax.o FNSP_Address.o FNSP_Impl.o

OBJECTS = fnsp_utils.o $(CTXOBJS) $(COMMONOBJS)

# include library definitions
include ../../../Makefile.libfn

LIBS = $(DYNLIBCCC)

LDLIBS += -lxfn -lfn_spf -lfn_p -lnsl -lC -lc

debug :=	CPPFLAGS += -g
tcov :=		CPPFLAGS += -a

.KEEP_STATE:

all: $(LIBS) 

debug install tcov: $(ROOTLIBDIR) $(ROOTLIBS) $(ROOTLINKSCCC)

install_h:

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

# include library targets
include ../../../Makefile.targ
