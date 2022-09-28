#
#ident	"@(#)Makefile	1.9	96/04/10 SMI"
#
# Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/libfn_spf/Makefile
#
LIBRARYCCC= libfn_spf.a
VERS = .1

HDRS= fn_spi.h FN_syntax_standard.h

HHDRS= fn_spi.hh \
	FN_syntax_standard.hh\
	FN_status_svc.hh FN_compound_name_standard.hh \
	FN_namelist_svc.hh FN_bindinglist_svc.hh \
	FN_ext_searchlist_svc.hh FN_searchlist_svc.hh \
	FN_multigetlist_svc.hh FN_valuelist_svc.hh

FNLINKS= fn_compound_name_standard.so$(VERS)

ROOTDIRS= $(ROOT)/usr/include/xfn
ROOTHDRS= $(HDRS:%=$(ROOTDIRS)/%)
FNHHDRS= $(HHDRS:%=$(FNINCDIR)/xfn/%)
CHECKHDRS= $(HDRS:%.h=%.check)

TRLOBJS= \
	FN_ctx_svc.o \
	FN_ctx_csvc.o \
	FN_ctx_asvc.o \
	FN_ctx_csvc_strong.o \
	FN_ctx_asvc_strong.o \
	FN_ctx_cnsvc.o \
	FN_ctx_cnsvc_impl.o \
	FN_ctx_cnsvc_weak_static.o \
	FN_ctx_cnsvc_weak_dynamic.o \
	FN_ctx_csvc_weak_static.o \
	FN_ctx_csvc_weak_dynamic.o \
	FN_ctx_asvc_weak.o \
	FN_ctx_asvc_weak_dynamic.o \
	FN_ctx_asvc_weak_static.o \
	FN_namelist_svc.o FN_bindinglist_svc.o \
	FN_multigetlist_svc.o FN_valuelist_svc.o \
	FN_searchlist_svc.o FN_ext_searchlist_svc.o \
	FN_status_svc.o \
	FN_compound_name_standard.o FN_syntax_standard.o \
	fn_links.o FN_ctx_func_info.o

OBJECTS= $(TRLOBJS)

# include library definitions
include ../Makefile.libfn

ROOTLIBDIR=	$(ROOT)/usr/lib
FNLINKTARG=	../$(DYNLIBCCC)

LIBS = $(DYNLIBCCC)

# C for C++
# xfn for client lib
LDLIBS += -lxfn -lC -lc 

#CPPFLAGS += -I$(SRC)/lib/libxfn

.KEEP_STATE:

all: install_h private_h $(LIBS)

install debug tcov: install_h private_h $(ROOTLIBDIR) $(ROOTLIBS) \
		$(ROOTLINKSCCC) $(ROOTFNLINKS)

install_h: $(ROOTDIRS) $(ROOTHDRS)

private_h: $(FNINCDIR) $(FNINCDIR)/xfn $(FNHHDRS)

check: $(CHECKHDRS)

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

# include library targets
include ../Makefile.targ
