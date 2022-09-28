#
#ident	"@(#)Makefile	1.6	96/03/18 SMI"	/* 	*/
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

FSTYPE=		lofs
LIBPROG=		mount
ATTMK= $(PROG)
OBJS=		mount.o fslib.o

include		../../Makefile.fstype

CPPFLAGS += -I../.. -o $@

lint := PROG =$(LIBPROG)

$(LIBPROG):	$(OBJS)
		$(LINK.c) $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

lint:	lint_PROG

fslib.o: ../../fslib.c
	$(COMPILE.c) ../../fslib.c

