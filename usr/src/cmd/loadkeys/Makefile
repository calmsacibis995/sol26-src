#
# @(#)Makefile 1.18 94/06/28 SMI
#
# Copyright (c) 1991, by Sun Microsystems, Inc.
#
# cmd/loadkeys/Makefile

PROG= loadkeys dumpkeys
sparc_SUBDIRS= type_4
ppc_SUBDIRS= type_101
i386_SUBDIRS= type_101
SUBDIRS= $($(MACH)_SUBDIRS)
SRCS=	$(PROG:%=%.c)

include ../Makefile.cmd

.KEEP_STATE:

.PARALLEL:	$(SUBDIRS)

all:  $(PROG) $(SUBDIRS)

install: $(PROG) $(ROOTPROG) $(SUBDIRS)

# explicit yacc work for the NSE
#
loadkeys.c: loadkeys.y
	$(YACC.y) loadkeys.y
	mv y.tab.c $@

loadkeys: loadkeys.c
	$(LINK.c) -o $@ loadkeys.c $(LDLIBS)
	$(POST_PROCESS)

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
_msg:=          TARGET= catalog

clobber:	$(SUBDIRS)
	$(RM) $(CLOBBERFILES)

clean:	$(SUBDIRS)
	$(RM) loadkeys.c

lint: $(SUBDIRS)
	$(LINT.c) $(SRCS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
