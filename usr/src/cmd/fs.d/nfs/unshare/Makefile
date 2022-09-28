#
#ident	"@(#)Makefile	1.12	96/04/30 SMI"	/* SVr4.0 1.12	*/
#
# Copyright (c) 1989,1995 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		nfs
LIBPROG=	unshare
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

COMMON=		sharetab.o
OBJS= 		$(LIBPROG).o $(COMMON) 
SRCS=		$(LIBPROG).c ../lib/sharetab.c
LDLIBS +=	-lnsl
CFLAGS += -o $@

$(LIBPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

sharetab.o:	../lib/sharetab.c
		$(COMPILE.c) ../lib/sharetab.c

lint:	lint_SRCS

clean:     
	$(RM) $(OBJS)
