#
#ident	"@(#)Makefile	1.10	96/04/30 SMI"	/* SVr4.0 1.12	*/
#
# Copyright (c) 1989, 1995, 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/nfs/nfsstat/Makefile

PROG=		nfsstat

include		../../../Makefile.cmd

CFLAGS +=	-v -o $@
COMMON=	nfs_sec.o
OBJS=	nfsstat.o $(COMMON)
SRCS=	nfsstat.c ../lib/nfs_sec.c

LDLIBS +=		-lkstat -lkvm -lelf -lnsl

$(PROG):	$(OBJS)
		$(LINK.c) -o $@ $(LDLIBS) $(OBJS)
		$(POST_PROCESS)

FILEMODE=       02555
GROUP=          sys

.KEEP_STATE:

all:		$(PROG)

nfs_sec.o:	../lib/nfs_sec.c
		$(COMPILE.c) ../lib/nfs_sec.c

install:	all $(ROOTPROG)

lint:	lint_SRCS

clean:     

include ../../../Makefile.targ
