#
#ident	"@(#)Makefile	1.23	96/04/22 SMI"
#
# Copyright (c) 1989 - 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# Makefile for the fn utilities
#
# cmd/fn/Makefile
#

PROG = \
    fnbind fnunbind fnlookup fnlist fnrename fnattr fncreate_printer fnsearch
SHFILE = fncheck fnsetup
SBINPROG = fncreate fncreate_fs fndestroy fncheck fnselect fncopy

CLOBBERFILES += $(SHFILE) $(SBINPROG)

include ../Makefile.cmd

ROOTPROGS	= $(PROG:%=$(ROOTBIN)/%)

ROOTSBINPROGS	= $(SBINPROG:%=$(ROOTUSRSBIN)/%)

SHSRCS=		$(SHFILE:%=%.sh)

LDLIBS += -lxfn

# CC adds /opt/SUNWspro/lib to the run_path it passes to ld.  This overrides
# it.
RUNPATH = -R/usr/lib
fncreate fncopy fncreate_printer := RUNPATH = -R/usr/lib/fn
fncreate fncopy fncreate_printer := LDLIBS += -L$(ROOTLIB)/fn
$(PROG) $(SBINPROG) := LDFLAGS += -Qoption ld $(RUNPATH)

FNONCSRC=	$(SRC)/lib/fn/context/onc_ns

# The following are for fncreate only
FNCOBJS = fncreate.o fnutils.o fncreate_attr_utils.o
HLIBS = -lfn_p -lnsl $(FNONCSRC)/nisplus/fn_ctx_onc_fn_nisplus.so.1 \
	$(FNONCSRC)/nis/fn_ctx_onc_fn_nis.so.1

# The following are for fncopy only
COBJS = fncopy.o fncreate_attr_utils.o fncopy_utils.o
CLIBS = -lfn_spf -lfn_p -lnsl $(FNONCSRC)/nisplus/fn_ctx_onc_fn_nisplus.so.1 \
	$(FNONCSRC)/nis/fn_ctx_onc_fn_nis.so.1

# The following are for fncreate_printer only
POBJS = fncreate_printer.o fncreate_attr_utils.o
PLIBS = -lfn_p -lnsl -lfnsp $(FNONCSRC)/nisplus/fn_ctx_onc_fn_nisplus.so.1 

# The following are for fncreate_fs only
FSOBJS = fncreate_fs.o fs_parse_utils.o fs_ctx_utils.o Tree.o
FSOBJS2 = fncreate_attr_utils.o 
FSLIBS = -lnsl -lfn_p
FSHDRS = $(SRC)/lib/fn/libxfn

FNINCDIR = $(SRC)/lib/fn/include
CPPFLAGS += -I$(FNINCDIR)

$(FSOBJS) :=	CPPFLAGS += -I$(FSHDRS)

debug :=	CPPFLAGS += $(DEBUG)
debug :=	COPTFLAG = -g
debug :=	CCOPTFLAG = -g
tcov :=		CFLAGS += -xa
tcov :=		CCFLAGS += -a

CLEANFILES += $(FNCOBJS) $(COBJS) $(FSOBJS) $(FSOBJS2) \
	$(PROG:%=%.o) fndestroy.o fnselect.o fnattr_utils.o

.KEEP_STATE:

all: $(PROG) $(SBINPROG)

debug install: all $(SHFILE) $(ROOTBIN) $(ROOTUSRSBIN) \
	$(ROOTPROG) $(ROOTSBINPROGS)

install_h:

%.o: %.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)

fncreate: $(FNCOBJS)
	$(LINK.cc) -o $@ $(FNCOBJS) $(LDLIBS) $(HLIBS)

fncreate_printer: $(POBJS)
	$(LINK.cc) -o $@ $(POBJS) $(LDLIBS) $(PLIBS)

fncreate_fs: $(FSOBJS) $(FSOBJS2)
	$(LINK.cc) -o $@ $(FSOBJS) $(FSOBJS2) $(LDLIBS) $(FSLIBS)

fndestroy: fndestroy.o
	$(LINK.cc) -o $@ fndestroy.o $(LDLIBS)

fnbind: fnbind.o
	$(LINK.cc) -o $@ fnbind.o $(LDLIBS) -lnsl

fnunbind: fnunbind.o
	$(LINK.cc) -o $@ fnunbind.o $(LDLIBS)

fnlookup: fnlookup.o
	$(LINK.cc) -o $@ fnlookup.o $(LDLIBS)

fnlist: fnlist.o
	$(LINK.cc) -o $@ fnlist.o $(LDLIBS)

fnattr: fnattr.o fnattr_utils.o
	$(LINK.cc) -o $@ fnattr.o fnattr_utils.o $(LDLIBS)

fnrename: fnrename.o 
	$(LINK.cc) -o $@ fnrename.o $(LDLIBS)

fnsearch: fnsearch.o fnattr_utils.o
	$(LINK.cc) -o $@ fnsearch.o fnattr_utils.o $(LDLIBS)

fnselect: fnselect.o 
	$(LINK.cc) -o $@ fnselect.o -lfn_p -lnsl $(LDLIBS)

fncopy: $(COBJS)
	$(LINK.cc) -o $@ $(COBJS) $(LDLIBS) $(CLIBS)

$(ROOTBIN) $(ROOTUSRSBIN) :
	$(INS.dir)

clean:
	-$(RM) $(CLEANFILES)

# disable linting since it is useless for C++ code
lint:

include ../Makefile.targ
