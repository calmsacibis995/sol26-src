#
#ident  "@(#)Makefile 1.23     96/03/18 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# cmd/backup/database/dbserv/Makefile
#

include ../../Makefile.backup

DUMPDBD=	rpc.dumpdbd

HSMPROG=	$(DUMPDBD)
PRODUCT=	$(HSMPROG)

OBJECTS=	dbserv_svc.o rserver_funcs.o batch_update.o dir_update.o \
		cache_subr.o instance_update.o trans_subr.o tape_subr.o \
		startup.o delete.o misc.o map_subr.o scandumps.o dbfind.o \
		listem.o dbinfo.o
SRCS=		$(OBJECTS:.o=.c)

LOCAL=		.
GENERAL=	../../include
CPPFLAGS=	-I$(LOCAL) -I$(GENERAL) $(USG) \
			-DHSMROOT=\"$(HSMROOT)\" $(CPPFLAGS.master)
CCMODE=		$(ANSI)
LIBDUMP=	../../lib/$(DUMPLIB)
HSMLDLIBS=	$(LDLIBS.cmd) $(LIBDUMP) -lsocket -lnsl

CLOBBERFILES=	$(PRODUCT) $(DEBUGPRODUCTS)

FILEMODE=	0555

.KEEP_STATE:

all hsm:	$(PRODUCT) 

$(HSMPROG):	$(OBJECTS) $$(LIBDUMP)
	$(LINK.c) -o $@ $(OBJECTS) $(HSMLDLIBS)
	$(POST_PROCESS)

$$(LIBDUMP): FRC
	$(DO_LIBDIR)

FRC:

install hsminstall:	hsm $(HSMROOTPKGLIBPROG)

lint:		lint_SRCS

clean:
	$(RM) $(OBJECTS) $(DEBUGOBJS)

include ../../Makefile.targ
