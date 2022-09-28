#
#ident	"@(#)Makefile	1.1	96/03/31 SMI"
#
# Copyright (c) 1993 - 1995 by Sun Microsystems, Inc.
#
# lib/fn/libxfn.1/Makefile
#
LIBRARYCCC= libxfn.a
#LIBRARY= libxfn.a
VERS = .1

ROOTDIRS= $(ROOT)/usr/include/xfn
ROOTHDRS= $(HDRS:%=$(ROOTDIRS)/%)
FNHHDRS= $(HHDRS:%=$(FNINCDIR)/xfn/%)
CHECKHDRS= $(HDRS:%.h=%.check)

COMPATOBJS = \
	FN_attribute_compat.o \
	FN_attrmodlist_compat.o \
	FN_attrset_compat.o \
	FN_composite_name_compat.o \
	FN_compound_name_compat.o \
	FN_ctx_compat.o \
	FN_ref_compat.o \
	FN_ref_addr_compat.o \
	FN_status_compat.o \
	FN_string_compat.o

INCOMPATOBJS = \
	FN_string_incompat.o \
	FN_ctx_incompat.o

OBJECTS= compat.o $(COMPATOBJS) $(INCOMPATOBJS)

# include library definitions
include ../Makefile.libfn

ROOTLIBDIR=	$(ROOT)/usr/lib

LIBS = $(DYNLIBCCC)

# dl for dlopen; 
# C C++
LDLIBS += -ldl -lC -lc

.KEEP_STATE:

all: install_h private_h $(LIBS)

install debug tcov: install_h private_h $(ROOTLIBDIR) $(ROOTLIBS)

install_h private_h: 

check: $(CHECKHDRS)

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

# include library targets
include ../Makefile.targ
