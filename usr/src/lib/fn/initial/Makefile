#
#ident	"@(#)Makefile	1.15	96/03/31 SMI"
#
# Copyright (c) 1989 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/initial/Makefile
#

LIBRARYCCC= fn_ctx_initial.a
VERS = .1

OBJECTS= Entry.o Table.o FNSP_InitialContext.o from_initial.o \
	FNSP_GlobalContext.o \
	FNSP_user_entries.o FNSP_host_entries.o FNSP_global_entries.o \
	FNSP_enterprise.o FNSP_enterprise_nisplus.o \
	FNSP_enterprise_nis.o FNSP_enterprise_files.o fnsp_nisplus_root.o

# include library definitions
include ../Makefile.libfn

LIBS = $(DYNLIBCCC)

LDLIBS += -lxfn -lfn_spf -lfn_p -lnsl -lC -lc

.KEEP_STATE:

all: $(LIBS)

debug install tcov: $(ROOTLIBDIR) $(ROOTLIBS)

install_h private_h check:

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

# for fnsp_nisplus_root.o
objs/%.o profs/%.o pics/%.o: ../context/onc_ns/nisplus/%.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)

# include library targets
include ../Makefile.targ
